/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "App/DeviceScanner.hpp"

#include "Utils/Common.hpp"

ebus::DeviceScanner::DeviceScanner(uint8_t ownAddress,
                                   DeviceManager* deviceManager)
    : deviceManager_(deviceManager),
      ownAddress_(ownAddress),
      nextStartupScanTime_(std::chrono::steady_clock::time_point::max()) {}

void ebus::DeviceScanner::setFullScan(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  fullScan_ = enable;
  if (enable) {
    fullScanAddress_ = 0;
  }
}

bool ebus::DeviceScanner::isFullScan() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return fullScan_;
}

void ebus::DeviceScanner::setScanOnStartup(bool enable) {
  std::lock_guard<std::mutex> lock(mutex_);
  scanOnStartup_ = enable;
  if (enable) {
    // Reset state and arm the timer for the first scan
    startupScanCount_ = 0;
    std::queue<std::vector<uint8_t>> empty;
    std::swap(startupQueue_, empty);
    nextStartupScanTime_ = std::chrono::steady_clock::now() + initialScanDelay_;
  }
}

bool ebus::DeviceScanner::isScanOnStartup() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scanOnStartup_;
}

void ebus::DeviceScanner::setOwnAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  ownAddress_ = address;
}

void ebus::DeviceScanner::setMaxStartupScans(uint8_t max) {
  std::lock_guard<std::mutex> lock(mutex_);
  maxStartupScans_ = max;
}

void ebus::DeviceScanner::setInitialScanDelay(std::chrono::seconds delay) {
  std::lock_guard<std::mutex> lock(mutex_);
  initialScanDelay_ = delay;
}

void ebus::DeviceScanner::setStartupScanInterval(
    std::chrono::seconds interval) {
  std::lock_guard<std::mutex> lock(mutex_);
  startupScanInterval_ = interval;
}

void ebus::DeviceScanner::scanObservedDevices() {
  // deviceManager is thread-safe, so we can query it outside our lock
  // to reduce contention, although getObservedSlaves copies the set anyway.
  std::set<uint8_t> slaves;
  std::vector<std::vector<uint8_t>> vendorCmds;

  if (deviceManager_) {
    slaves = deviceManager_->getObservedSlaves();
    vendorCmds = deviceManager_->vendorScanCommands();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  for (uint8_t addr : slaves) {
    scanAddressLocked(addr);
  }
  // Also queue vendor-specific scans for a complete refresh
  for (const auto& cmd : vendorCmds) {
    // Basic deduplication: only add if the queue is small or command is unique
    manualQueue_.push(cmd);
  }
}

void ebus::DeviceScanner::scanAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  scanAddressLocked(address);
}

void ebus::DeviceScanner::scanAddressLocked(uint8_t address) {
  if (ebus::isSlave(address) && (address != ebus::slaveOf(ownAddress_))) {
    manualQueue_.push(Device::createScanCommand(address));
  }
}

void ebus::DeviceScanner::scanAddresses(const std::vector<uint8_t>& addresses) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (uint8_t addr : addresses) {
    scanAddressLocked(addr);
  }
}

bool ebus::DeviceScanner::isScanning() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return fullScan_ || scanOnStartup_ || !manualQueue_.empty() ||
         !startupQueue_.empty();
}

void ebus::DeviceScanner::stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  fullScan_ = false;
  scanOnStartup_ = false;

  std::queue<std::vector<uint8_t>> emptyManual;
  std::swap(manualQueue_, emptyManual);

  std::queue<std::vector<uint8_t>> emptyStartup;
  std::swap(startupQueue_, emptyStartup);

  nextStartupScanTime_ = std::chrono::steady_clock::time_point::max();
}

std::vector<uint8_t> ebus::DeviceScanner::nextCommand() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Priority 1: Manual Scan
  if (!manualQueue_.empty()) {
    auto cmd = manualQueue_.front();
    manualQueue_.pop();
    return cmd;
  }

  // Priority 2: Full Scan (generates one command at a time)
  if (fullScan_) {
    while (fullScanAddress_ <= 0xff) {
      uint8_t addr = static_cast<uint8_t>(fullScanAddress_);
      fullScanAddress_++;

      if (ebus::isSlave(addr) && (addr != ebus::slaveOf(ownAddress_))) {
        return Device::createScanCommand(addr);
      }
    }
    // Finished full scan
    fullScan_ = false;
  }

  // Priority 3: Startup Scan (Discovery of observed devices)
  if (scanOnStartup_) {
    // If the queue for the current iteration is empty, try to populate it.
    if (startupQueue_.empty()) {
      // Stop if we have completed all scan iterations.
      if (startupScanCount_ >= maxStartupScans_) {
        scanOnStartup_ = false;
        return {};
      }

      auto now = std::chrono::steady_clock::now();
      // Check if it's time for the next iteration.
      if (now >= nextStartupScanTime_) {
        startupScanCount_++;

        std::set<uint8_t> targets;
        if (deviceManager_) {
          // Note: accessing deviceManager under lock.
          // deviceManager handles its own locking, so this is safe.
          targets = deviceManager_->getObservedSlaves();
        }

        // Populate queue for this iteration
        for (uint8_t addr : targets) {
          startupQueue_.push(Device::createScanCommand(addr));
        }
        // Also queue vendor-specific scans for already identified devices
        if (deviceManager_) {
          auto vendorCmds = deviceManager_->vendorScanCommands();
          for (const auto& vcmd : vendorCmds) {
            startupQueue_.push(vcmd);
          }
        }

        // Schedule the next iteration.
        nextStartupScanTime_ = now + startupScanInterval_;
      }
    }

    if (!startupQueue_.empty()) {
      auto cmd = startupQueue_.front();
      startupQueue_.pop();
      return cmd;
    }
  }

  return {};
}