/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_scanner.hpp"

#include <algorithm>
#include <bitset>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <iterator>

namespace ebus::detail {

DeviceScanner::DeviceScanner(uint8_t address, DeviceManager* device_manager)
    : device_manager_(device_manager),
      own_address_(address),
      next_startup_scan_time_(Clock::time_point::min()),
      startup_observed_cursor_(0),
      startup_current_device_addr_(256),
      startup_current_device_id_scan_issued_(false),
      startup_current_device_vendor_cursor_(0) {
  // Note: current_deep_scan_address_ and other members are initialized
  // via default member initializers in the header.
  scan_attempt_counters_.fill(0);
}

void DeviceScanner::stop() {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  full_scan_ = false;
  pending_deep_scans_.reset();
  failed_scans_.reset();
  quarantined_scans_.reset();
  last_failure_reset_time_ = Clock::time_point::min();
  scan_attempt_counters_.fill(0);
  current_deep_scan_address_ = 256;
  startup_iteration_active_ = false;
}

void DeviceScanner::setOwnAddress(uint8_t address) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  own_address_ = address;
}

void DeviceScanner::setScanOnStartup(bool enable) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  scan_on_startup_ = enable;
  if (enable) {
    // Reset state and arm the timer for the first scan
    startup_scan_count_ = 0;
    startup_iteration_active_ = true;  // Start immediately if delay is 0
    startup_observed_cursor_ = 0;
    startup_current_device_addr_ = 256;
    startup_current_device_id_scan_issued_ = false;
    startup_current_device_vendor_cursor_ = 0;
    next_startup_scan_time_ = Clock::now() + initial_scan_delay_;
  }
}

bool DeviceScanner::isScanOnStartup() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return scan_on_startup_;
}

void DeviceScanner::setMaxStartupScans(uint8_t max) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  max_startup_scans_ = max;
}

void DeviceScanner::setInitialScanDelay(uint32_t delay_s) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  initial_scan_delay_ = std::chrono::seconds(delay_s);
}

void DeviceScanner::setStartupScanInterval(uint32_t interval_s) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  startup_scan_interval_ = std::chrono::seconds(interval_s);
}

void DeviceScanner::setBusyPredicate(Delegate<bool()> pred) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  is_busy_ = std::move(pred);
}

void DeviceScanner::initFullScan(bool enable) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  full_scan_ = enable;
  if (enable) {
    // Start full-scan from beginning without touching startup timing/state.
    full_scan_address_ = 0;
  }
}

bool DeviceScanner::scanObservedDevices() {
  if (!device_manager_) {
    return false;
  }

  std::bitset<256> observed;
  device_manager_->getObservedSlaves(observed);

  bool any_added = false;
  for (size_t i = 0; i < 256; ++i) {
    if (observed.test(i)) {
      if (scanAddressInternal(static_cast<uint8_t>(i))) any_added = true;
    }
  }
  return any_added;
}

bool DeviceScanner::scanAddress(uint8_t address) {
  return scanAddressInternal(address);
}

bool DeviceScanner::scanAddresses(const std::vector<uint8_t>& addresses) {
  bool all_success = true;
  for (uint8_t addr : addresses) {
    if (!scanAddressInternal(addr)) {
      all_success = false;
    }
  }
  return all_success;
}

ebus::Sequence DeviceScanner::nextCommand() {
  platform::UniqueLock<platform::Mutex> lock(mutex_);

  if (!device_manager_) {
    return {};
  }

  while (true) {
    const auto now = Clock::now();

    // 1. Batch Reset: Clear short-term failure cooldowns every 60 seconds.
    if (failed_scans_.any() &&
        (now - last_failure_reset_time_ > scan_cooldown_duration_)) {
      failed_scans_.reset();
      last_failure_reset_time_ = now;
    }

    // 2. Epoch Reset: Heavy cleanup of counters/metrics every 30 minutes of
    // activity.
    if (now - last_scan_attempt_ > std::chrono::minutes(30)) {
      failed_scans_.reset();
      quarantined_scans_.reset();
      scan_attempt_counters_.fill(0);
      failure_resets_++;
    }

    // Priority Logic: Background discovery (Full/Startup) is postponed if busy.
    const bool system_busy = is_busy_ && is_busy_();

    // Priority 1: Deep Scan (Manual, Observed, Startup)
    // Iterates through addresses requiring full identification.
    while (current_deep_scan_address_ < 256 || pending_deep_scans_.any()) {
      if (current_deep_scan_address_ == 256) {
        for (uint16_t i = 0; i < 256; ++i) {
          if (pending_deep_scans_.test(i)) {
            current_deep_scan_address_ = i;
            current_deep_scan_vendor_cursor_ = 0;
            pending_deep_scans_.reset(i);
            break;
          }
        }
      }

      if (current_deep_scan_address_ == 256) break;

      uint8_t addr = static_cast<uint8_t>(current_deep_scan_address_);
      if (failed_scans_.test(addr) || quarantined_scans_.test(addr)) {
        current_deep_scan_address_ = 256;
        continue;
      }

      Sequence cmd;
      if (!device_manager_->isIdentified(addr)) {
        cmd = Device::createScanCommand(addr);
        if (!cmd.empty()) {
          last_scan_attempt_ = now;
          current_deep_scan_address_ = 256;  // Reset to move to next task
          return cmd;
        }
      } else if (device_manager_->getNextPendingVendorCommandForDevice(
                     addr, current_deep_scan_vendor_cursor_, cmd)) {
        last_scan_attempt_ = now;
        return cmd;
      }

      // Device fully scanned or unreachable
      current_deep_scan_address_ = 256;
    }

    if (system_busy) return {};

    // Priority 2: Full Scan (Surface mapping of the whole bus)
    if (full_scan_) {
      while (full_scan_address_ < 256) {
        uint8_t addr = static_cast<uint8_t>(full_scan_address_++);

        // Full scan only probes slaves and skips our own address
        if (!ebus::isSlave(addr) || addr == ebus::slaveOf(own_address_))
          continue;

        if (failed_scans_.test(addr) || quarantined_scans_.test(addr)) continue;

        if (device_manager_ && !device_manager_->isIdentified(addr)) {
          auto cmd = Device::createScanCommand(addr);
          if (!cmd.empty()) {
            last_scan_attempt_ = now;
            return cmd;
          }
        }
      }
      full_scan_ = false;
    }

    // Priority 3: Startup Scan (Pass over all observed devices)
    if (scan_on_startup_ && now >= next_startup_scan_time_) {
      if (startup_scan_count_ >= max_startup_scans_) {
        scan_on_startup_ = false;
      } else {
        lock.unlock();
        bool added = scanObservedDevices();
        lock.lock();

        startup_scan_count_++;
        next_startup_scan_time_ = now + startup_scan_interval_;
        if (added) continue;  // Loop back to pick up the newly set bits
      }
    }

    break;
  }

  return {};
}

void DeviceScanner::onScanResult(uint8_t address, bool success) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (!success) {
    failed_scans_.set(address);
    // If the device fails repeatedly even after short cooldowns, quarantine it
    if (++scan_attempt_counters_[address] >= 10) {
      quarantined_scans_.set(address);
      scan_attempt_counters_[address] = 0;
    }
  } else {
    failed_scans_.reset(address);

    // Chaining logic: if a scan succeeded, check if we need to immediately
    // move to the next phase of identification (e.g. vendor commands).
    if (device_manager_ && device_manager_->needsDeepScan(address)) {
      // Safety limit: if we've tried identifying this device too many times
      // without it becoming fully profiled, stop re-enqueuing to prevent loops.
      if (++scan_attempt_counters_[address] < 10) {
        pending_deep_scans_.set(address);
      } else {
        // Faulty or non-compliant device: quarantine until major reset.
        quarantined_scans_.set(address);
        scan_attempt_counters_[address] = 0;
      }
    } else {
      scan_attempt_counters_[address] = 0;  // Reset counter on full success
    }
  }
}

void DeviceScanner::resetPeakMetrics() {}

bool DeviceScanner::isFullScan() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return full_scan_;
}

bool DeviceScanner::isScanning() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return full_scan_ || scan_on_startup_ || pending_deep_scans_.any();
}

DeviceScannerStatus DeviceScanner::fetchStatus() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  DeviceScannerStatus s;
  s.is_scanning = full_scan_ || scan_on_startup_ || pending_deep_scans_.any();
  s.full_scan_active = full_scan_;
  s.full_scan_address = full_scan_address_;
  s.scan_on_startup_enabled = scan_on_startup_;
  s.startup_scan_count = startup_scan_count_;
  s.startup_iteration_active = startup_iteration_active_;
  s.startup_current_device_addr = startup_current_device_addr_;
  s.pending_deep_scans = pending_deep_scans_.count();
  s.failed_scans = failed_scans_.count();
  s.quarantined_scans = quarantined_scans_.count();
  s.failure_resets = failure_resets_;
  return s;
}

bool DeviceScanner::scanAddressInternal(uint8_t address) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (address == ebus::slaveOf(own_address_)) return false;
  if (pending_deep_scans_.test(address)) return true;  // Already pending

  pending_deep_scans_.set(address);
  // Reset cooldown for manually requested scans
  failed_scans_.reset(address);
  quarantined_scans_.reset(address);
  return true;
}

}  // namespace ebus::detail