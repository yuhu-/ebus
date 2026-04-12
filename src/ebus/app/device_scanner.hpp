/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Generates scan commands to discover devices on the eBUS. It acts as a state
// machine that, when prompted, provides the next command to be sent. It
// supports three modes with a clear priority: Manual Scan > Full Scan >
// Startup Scan.

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

#include "app/device_manager.hpp"
#include "core/handler.hpp"

namespace ebus {

class DeviceScanner {
 public:
  DeviceScanner(uint8_t ownAddress, DeviceManager* deviceManager);

  void setFullScan(bool enable);
  bool isFullScan() const;

  void setScanOnStartup(bool enable);
  void setOwnAddress(uint8_t address);
  bool isScanOnStartup() const;
  void setMaxStartupScans(uint8_t max);
  void setInitialScanDelay(std::chrono::seconds delay);
  void setStartupScanInterval(std::chrono::seconds interval);

  // Manual scanning
  void scanObservedDevices();
  void scanAddress(uint8_t address);
  void scanAddresses(const std::vector<uint8_t>& addresses);

  bool isScanning() const;
  void stop();

  // Returns the next command to send, or empty vector if idle
  std::vector<uint8_t> nextCommand();

 private:
  DeviceManager* deviceManager_ = nullptr;
  uint8_t ownAddress_ = 0xff;

  // Protects all internal state across threads (Controller and Scheduler)
  mutable std::mutex mutex_;

  // Commands explicitly requested via scanAddress() or scanObservedDevices().
  // These are always returned first.
  std::queue<std::vector<uint8_t>> manualQueue_;

  // Timing configuration for the discovery/startup phase.
  std::chrono::seconds initialScanDelay_{10};
  std::chrono::seconds startupScanInterval_{60};

  // The wall-clock time when the next startup scan iteration is allowed to run
  std::chrono::steady_clock::time_point nextStartupScanTime_;

  // Flag and cursor for the exhaustive 0x00-0xff scan.
  // fullScanAddress_ iterates from 0 to 255.
  bool fullScan_ = false;
  uint16_t fullScanAddress_ = 0;

  // Configuration for the background discovery of known devices.
  bool scanOnStartup_ = false;
  // Number of full discovery iterations performed so far
  uint8_t startupScanCount_ = 0;
  // Threshold to stop periodic discovery
  uint8_t maxStartupScans_ = 5;
  // Buffer of commands for the currently active startup scan iteration
  std::queue<std::vector<uint8_t>> startupQueue_;

  // Internal helper to add a scan command without locking (caller must hold
  // lock)
  void scanAddressLocked(uint8_t address);
};

}  // namespace ebus