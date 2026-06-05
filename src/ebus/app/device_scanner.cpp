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
      next_startup_scan_time_(Clock::time_point::max()) {}

void DeviceScanner::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Stop manual scanning and any active full scan, but preserve startup scans.
  full_scan_ = false;
  manual_queue_.clear();
}

void DeviceScanner::setOwnAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  own_address_ = address;

  // Purge any pending manual retries that now target our own slave address
  // to prevent self-probing after an address change.
  StaticVector<Sequence, DeviceLimits::max_manual_queue> filtered;
  const uint8_t own_slave = ebus::slaveOf(address);
  while (!manual_queue_.empty()) {
    auto cmd = std::move(manual_queue_[0]);
    if (!cmd.empty() && cmd[0] != own_slave) filtered.push_back(std::move(cmd));
    manual_queue_.erase(manual_queue_.begin());
  }
  manual_queue_ = std::move(filtered);
}

void DeviceScanner::setScanOnStartup(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  scan_on_startup_ = enable;
  if (enable) {
    // Reset state and arm the timer for the first scan
    startup_scan_count_ = 0;
    startup_queue_.clear();
    next_startup_scan_time_ = Clock::now() + initial_scan_delay_;
  }
}

bool DeviceScanner::isScanOnStartup() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scan_on_startup_;
}

void DeviceScanner::setMaxStartupScans(uint8_t max) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_startup_scans_ = max;
}

void DeviceScanner::setInitialScanDelay(uint32_t delay_s) {
  std::lock_guard<std::mutex> lock(mutex_);
  initial_scan_delay_ = std::chrono::seconds(delay_s);
}

void DeviceScanner::setStartupScanInterval(uint32_t interval_s) {
  std::lock_guard<std::mutex> lock(mutex_);
  startup_scan_interval_ = std::chrono::seconds(interval_s);
}

void DeviceScanner::initFullScan(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  full_scan_ = enable;
  if (enable) {
    // Start full-scan from beginning without touching startup timing/state.
    full_scan_address_ = 0;
  } else {
    // Stop full scan; leave startup scans untouched.
    full_scan_ = false;
  }
}

bool DeviceScanner::scanObservedDevices() {
  bool queued_any = false;
  // device_manager is thread-safe, so we can query it outside our lock
  // to reduce contention, although getObservedSlaves copies the set anyway.
  std::bitset<256> observed;
  std::vector<Sequence> vendor_cmds_buffer;

  if (device_manager_) {
    device_manager_->getObservedSlaves(observed);
    device_manager_->vendorScanCommands(
        [&](const Sequence& cmd) { vendor_cmds_buffer.push_back(cmd); });
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (size_t i = 0; i < 256; ++i) {
    if (observed.test(i)) {
      if (scanAddressLocked(static_cast<uint8_t>(i))) queued_any = true;
    }
  }
  // Also queue vendor-specific scans for a complete refresh
  for (const auto& cmd : vendor_cmds_buffer) {
    if (cmd.empty()) continue;
    if (manual_queue_.push_back(cmd)) {
      queued_any = true;
      if (manual_queue_.size() > max_manual_queue_size_) {
        max_manual_queue_size_ = manual_queue_.size();
      }
    }
  }
  return queued_any;
}

bool DeviceScanner::scanAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  return scanAddressLocked(address);
}

bool DeviceScanner::scanAddresses(const std::vector<uint8_t>& addresses) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool all_success = true;
  for (uint8_t addr : addresses) {
    if (!scanAddressLocked(addr)) {
      all_success = false;
    }
  }
  return all_success;
}

ebus::Sequence DeviceScanner::nextCommand() {
  const auto now = Clock::now();
  std::unique_lock<std::mutex> lock(mutex_);

  // Priority 1: Manual Scan
  if (!manual_queue_.empty()) {
    auto cmd = std::move(manual_queue_[0]);
    manual_queue_.erase(manual_queue_.begin());
    if (!cmd.empty()) return cmd;
  }

  // Priority 2: Full Scan (independent from startup timing)
  if (full_scan_) {
    while (full_scan_address_ < 256) {
      uint8_t addr = static_cast<uint8_t>(full_scan_address_++);
      // Note: we increment the cursor once per attempt; adjust to ensure
      // progress even if createScanCommand returns empty.
      if (ebus::isSlave(addr) && (addr != ebus::slaveOf(own_address_))) {
        auto cmd = Device::createScanCommand(addr);
        if (!cmd.empty()) return cmd;
        // If createScanCommand returned empty, continue the loop.
      }
    }
    // Finished full scan
    full_scan_ = false;
  }

  // Priority 3: Startup Scan (Discovery of observed devices)
  if (!scan_on_startup_) return {};

  if (now < next_startup_scan_time_) {
    // If we are still in initial delay, but there might be leftover
    // startup_queue_
    if (!startup_queue_.empty()) {
      auto cmd = std::move(startup_queue_[0]);
      startup_queue_.erase(startup_queue_.begin());
      if (!cmd.empty()) return cmd;
      return {};
    }
    return {};
  }

  if (startup_queue_.empty()) {
    // Stop if we have completed all scan iterations.
    if (startup_scan_count_ >= max_startup_scans_) {
      scan_on_startup_ = false;
      return {};
    }

    // Trigger iteration: drop lock to query thread-safe DeviceManager
    lock.unlock();
    std::bitset<256> observed;
    std::vector<Sequence> vendor_cmds_buffer;
    if (device_manager_) {
      device_manager_->getObservedSlaves(observed);
      device_manager_->vendorScanCommands(
          [&](const Sequence& cmd) { vendor_cmds_buffer.push_back(cmd); });
    }

    // Populate iteration queue outside the lock
    std::vector<Sequence> temp_q;
    for (size_t i = 0; i < 256; ++i) {
      if (observed.test(i)) {
        auto cmd = Device::createScanCommand(static_cast<uint8_t>(i));
        if (!cmd.empty()) temp_q.push_back(std::move(cmd));
      }
    }
    std::copy_if(vendor_cmds_buffer.begin(), vendor_cmds_buffer.end(),
                 std::back_inserter(temp_q),
                 [](const Sequence& cmd) { return !cmd.empty(); });

    lock.lock();
    // Verify state hasn't changed (e.g. stop() called) while we were unlocked
    if (scan_on_startup_ && startup_queue_.empty() &&
        startup_scan_count_ < max_startup_scans_) {
      for (auto& cmd : temp_q) {
        if (!startup_queue_.push_back(std::move(cmd))) break;
      }
      if (startup_queue_.size() > max_startup_queue_size_) {
        max_startup_queue_size_ = startup_queue_.size();
      }
      startup_scan_count_++;
      next_startup_scan_time_ = now + startup_scan_interval_;
    }
  }

  if (!startup_queue_.empty()) {
    auto cmd = std::move(startup_queue_[0]);
    startup_queue_.erase(startup_queue_.begin());
    if (!cmd.empty()) return cmd;
  }

  return {};
}

void DeviceScanner::resetPeakMetrics() {
  std::lock_guard<std::mutex> lock(mutex_);
  max_manual_queue_size_ = manual_queue_.size();
  max_startup_queue_size_ = startup_queue_.size();
}

bool DeviceScanner::isFullScan() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_scan_;
}

bool DeviceScanner::isScanning() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_scan_ || scan_on_startup_ || !manual_queue_.empty() ||
         !startup_queue_.empty();
}

DeviceScannerStatus DeviceScanner::getStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  DeviceScannerStatus s;
  s.is_scanning = full_scan_ || scan_on_startup_ || !manual_queue_.empty() ||
                  !startup_queue_.empty();
  s.full_scan_active = full_scan_;
  s.full_scan_address = full_scan_address_;
  s.scan_on_startup_enabled = scan_on_startup_;
  s.startup_scan_count = startup_scan_count_;
  s.manual_queue_size = manual_queue_.size();
  s.max_manual_queue_size = max_manual_queue_size_;
  s.startup_queue_size = startup_queue_.size();
  s.max_startup_queue_size = max_startup_queue_size_;
  return s;
}

bool DeviceScanner::scanAddressLocked(uint8_t address) {
  if (ebus::isSlave(address) && (address != ebus::slaveOf(own_address_))) {
    auto cmd = Device::createScanCommand(address);
    if (cmd.empty()) return false;
    bool pushed = manual_queue_.push_back(std::move(cmd));
    if (pushed && manual_queue_.size() > max_manual_queue_size_) {
      max_manual_queue_size_ = manual_queue_.size();
    }
    return pushed;
  }
  return false;
}

}  // namespace ebus::detail