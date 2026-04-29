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

void DeviceScanner::setOwnAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  own_address_ = address;

  // Purge any pending manual retries that now target our own slave address
  // to prevent self-probing after an address change.
  std::queue<Sequence> filtered;
  const uint8_t own_slave = ebus::slaveOf(address);
  while (!manual_queue_.empty()) {
    auto& cmd = manual_queue_.front();
    if (cmd.empty() || cmd[0] != own_slave) filtered.push(std::move(cmd));
    manual_queue_.pop();
  }
  manual_queue_ = std::move(filtered);
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

void DeviceScanner::setFullScan(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  full_scan_ = enable;
  if (enable) {
    full_scan_address_ = 0;
    // Arm the timer for the initial delay if not already active
    if (next_startup_scan_time_ ==
        std::chrono::steady_clock::time_point::max()) {
      next_startup_scan_time_ =
          std::chrono::steady_clock::now() + initial_scan_delay_;
    }
  }
}

bool DeviceScanner::isFullScan() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return full_scan_;
}

bool DeviceScanner::scanObservedDevices() {
  bool queued_any = false;
  // device_manager is thread-safe, so we can query it outside our lock
  // to reduce contention, although getObservedSlaves copies the set anyway.
  std::bitset<256> observed;
  std::vector<Sequence> vendor_cmds;  // Correct type

  if (device_manager_) {
    device_manager_->getObservedSlaves(observed);
    vendor_cmds = device_manager_->vendorScanCommands();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (size_t i = 0; i < 256; ++i) {
    if (observed.test(i)) {
      if (scanAddressLocked(static_cast<uint8_t>(i))) queued_any = true;
    }
  }
  // Also queue vendor-specific scans for a complete refresh
  for (const auto& cmd : vendor_cmds) {
    // Basic deduplication: only add if the queue is small or command is unique
    if (manual_queue_.size() < ScannerLimits::max_manual_queue) {
      manual_queue_.push(cmd);
      queued_any = true;
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
  const auto now = std::chrono::steady_clock::now();
  std::unique_lock<std::mutex> lock(mutex_);

  // Priority 1: Manual Scan
  if (!manual_queue_.empty()) {
    auto cmd = std::move(manual_queue_.front());
    manual_queue_.pop();
    return cmd;
  }

  // Autonomous scans (Full and Startup) must respect the initial delay
  // guard
  if (now < next_startup_scan_time_) {
    // Even if we are in the delay phase, we might still have items left in
    // the startup_queue from a previous trigger.
    if (scan_on_startup_ && !startup_queue_.empty()) {
      auto cmd = std::move(startup_queue_.front());
      startup_queue_.pop();
      return cmd;
    }
    return {};
  }

  // Priority 2: Full Scan (generates one command at a time)
  if (full_scan_) {
    while (full_scan_address_ <= 0xff) {
      uint8_t addr = static_cast<uint8_t>(full_scan_address_++);
      if (ebus::isSlave(addr) && (addr != ebus::slaveOf(own_address_))) {
        return Device::createScanCommand(addr);
      }
    }
    // Finished full scan
    full_scan_ = false;
  }

  // Priority 3: Startup Scan (Discovery of observed devices)
  if (!scan_on_startup_) return {};

  if (startup_queue_.empty()) {
    // Stop if we have completed all scan iterations.
    if (startup_scan_count_ >= max_startup_scans_) {
      scan_on_startup_ = false;
      return {};
    }

    // Trigger iteration: drop lock to query thread-safe DeviceManager
    lock.unlock();
    std::bitset<256> observed;
    std::vector<Sequence> vendor_cmds;
    if (device_manager_) {
      device_manager_->getObservedSlaves(observed);
      vendor_cmds = device_manager_->vendorScanCommands();
    }

    // Populate iteration queue outside the lock
    std::queue<Sequence> temp_q;
    for (size_t i = 0; i < 256; ++i) {
      if (observed.test(i)) {
        temp_q.push(Device::createScanCommand(static_cast<uint8_t>(i)));
      }
    }
    for (const auto& vcmd : vendor_cmds) {
      temp_q.push(vcmd);
    }

    lock.lock();
    // Verify state hasn't changed (e.g. stop() called) while we were unlocked
    if (scan_on_startup_ && startup_queue_.empty() &&
        startup_scan_count_ < max_startup_scans_) {
      startup_queue_ = std::move(temp_q);
      startup_scan_count_++;
      next_startup_scan_time_ = now + startup_scan_interval_;
    }
  }

  if (!startup_queue_.empty()) {
    auto cmd = std::move(startup_queue_.front());
    startup_queue_.pop();
    return cmd;
  }

  return {};
}

bool DeviceScanner::scanAddressLocked(uint8_t address) {
  if (ebus::isSlave(address) && (address != ebus::slaveOf(own_address_))) {
    if (manual_queue_.size() >= ScannerLimits::max_manual_queue) return false;
    manual_queue_.push(Device::createScanCommand(address));
    return true;
  }
  return false;
}

}  // namespace ebus::detail