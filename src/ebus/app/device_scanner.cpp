/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_scanner.hpp"

#include <bitset>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

DeviceScanner::DeviceScanner(uint8_t address, DeviceManager* device_manager)
    : device_manager_(device_manager),
      own_address_(address),
      next_startup_scan_time_(std::chrono::steady_clock::time_point::max()) {}

void DeviceScanner::setFullScan(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  full_scan_ = enable;
  if (enable) {
    full_scan_address_ = 0;
  }
}

bool DeviceScanner::isFullScan() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_scan_;
}

void DeviceScanner::setScanOnStartup(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  scan_on_startup_ = enable;
  if (enable) {
    // Reset state and arm the timer for the first scan
    startup_scan_count_ = 0;
    std::queue<Sequence> empty;
    std::swap(startup_queue_, empty);
    next_startup_scan_time_ =
        std::chrono::steady_clock::now() + initial_scan_delay_;
  }
}

bool DeviceScanner::isScanOnStartup() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scan_on_startup_;
}

void DeviceScanner::setOwnAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  own_address_ = address;
}

void DeviceScanner::setMaxStartupScans(uint8_t max) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_startup_scans_ = max;
}

void DeviceScanner::setInitialScanDelay(std::chrono::seconds delay) {
  std::lock_guard<std::mutex> lock(mutex_);
  initial_scan_delay_ = delay;
}

void DeviceScanner::setStartupScanInterval(std::chrono::seconds interval) {
  std::lock_guard<std::mutex> lock(mutex_);
  startup_scan_interval_ = interval;
}

void DeviceScanner::scanObservedDevices() {
  // device_manager is thread-safe, so we can query it outside our lock
  // to reduce contention, although getObservedSlaves copies the set anyway.
  std::bitset<256> observed;
  std::vector<Sequence> vendor_cmds;  // Correct type

  if (device_manager_) {
    observed = device_manager_->getObservedSlaves();
    vendor_cmds = device_manager_->vendorScanCommands();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (size_t i = 0; i < 256; ++i) {
    if (observed.test(i)) {
      scanAddressLocked(static_cast<uint8_t>(i));
    }
  }
  // Also queue vendor-specific scans for a complete refresh
  for (const auto& cmd : vendor_cmds) {
    // Basic deduplication: only add if the queue is small or command is unique
    manual_queue_.push(cmd);
  }
}

void DeviceScanner::scanAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  scanAddressLocked(address);
}

void DeviceScanner::scanAddressLocked(uint8_t address) {
  if (ebus::isSlave(address) && (address != ebus::slaveOf(own_address_))) {
    manual_queue_.push(Device::createScanCommand(address));
  }
}

void DeviceScanner::scanAddresses(const std::vector<uint8_t>& addresses) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (uint8_t addr : addresses) {
    scanAddressLocked(addr);
  }
}

bool DeviceScanner::isScanning() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_scan_ || scan_on_startup_ || !manual_queue_.empty() ||
         !startup_queue_.empty();
}

void DeviceScanner::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  full_scan_ = false;
  scan_on_startup_ = false;

  std::queue<Sequence> empty_manual;
  std::swap(manual_queue_, empty_manual);

  std::queue<Sequence> empty_startup;
  std::swap(startup_queue_, empty_startup);

  next_startup_scan_time_ = std::chrono::steady_clock::time_point::max();
}

ebus::Sequence DeviceScanner::nextCommand() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Priority 1: Manual Scan
  if (!manual_queue_.empty()) {
    auto cmd = manual_queue_.front();
    manual_queue_.pop();
    return cmd;
  }

  // Priority 2: Full Scan (generates one command at a time)
  if (full_scan_) {
    while (full_scan_address_ <= 0xff) {
      uint8_t addr = static_cast<uint8_t>(full_scan_address_);
      full_scan_address_++;

      if (ebus::isSlave(addr) && (addr != ebus::slaveOf(own_address_))) {
        return Device::createScanCommand(addr);
      }
    }
    // Finished full scan
    full_scan_ = false;
  }

  // Priority 3: Startup Scan (Discovery of observed devices)
  if (scan_on_startup_) {
    // If the queue for the current iteration is empty, try to populate it.
    if (startup_queue_.empty()) {
      // Stop if we have completed all scan iterations.
      if (startup_scan_count_ >= max_startup_scans_) {
        scan_on_startup_ = false;
        return {};
      }

      auto now = std::chrono::steady_clock::now();
      // Check if it's time for the next iteration.
      if (now >= next_startup_scan_time_) {
        startup_scan_count_++;

        std::bitset<256> targets;
        if (device_manager_) {
          // Note: accessing device_manager under lock.
          // device_manager handles its own locking, so this is safe.
          targets = device_manager_->getObservedSlaves();
        }

        // Populate queue for this iteration
        for (size_t i = 0; i < 256; ++i) {
          if (targets.test(i)) {
            startup_queue_.push(
                Device::createScanCommand(static_cast<uint8_t>(i)));
          }
        }
        // Also queue vendor-specific scans for already identified devices
        if (device_manager_) {
          auto vendor_cmds = device_manager_->vendorScanCommands();
          for (const auto& vcmd : vendor_cmds) {
            startup_queue_.push(vcmd);
          }
        }

        // Schedule the next iteration.
        next_startup_scan_time_ = now + startup_scan_interval_;
      }
    }

    if (!startup_queue_.empty()) {
      auto cmd = startup_queue_.front();
      startup_queue_.pop();
      return cmd;
    }
  }

  return {};
}

}  // namespace ebus::detail