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
#include <ebus/sequence.hpp>
#include <mutex>
#include <queue>
#include <vector>

#include "app/device_manager.hpp"
#include "core/constants.hpp"
#include "core/handler.hpp"

namespace ebus {

/**
 * The DeviceScanner is responsible for generating eBUS scan commands to
 * discover devices on the bus. It operates as a state machine that prioritizes
 * three scanning modes: Manual Scan (initiated by explicit requests), Full Scan
 * (exhaustive 0x00-0xff), and Startup Scan (periodic discovery of known
 * devices). The scanner maintains internal queues for manual and startup scans.
 */
class DeviceScanner {
 public:
  DeviceScanner(uint8_t address, DeviceManager* device_manager);

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
  Sequence nextCommand();

 private:
  DeviceManager* device_manager_ = nullptr;
  uint8_t own_address_ = defaults::address;

  // Protects all internal state across threads (Controller and Scheduler)
  mutable std::mutex mutex_;

  // Commands explicitly requested via scanAddress() or scanObservedDevices().
  // These are always returned first.
  std::queue<Sequence> manual_queue_;

  // Timing configuration for the discovery/startup phase.
  std::chrono::seconds initial_scan_delay_{internal::Scanner::initial_delay_s};
  std::chrono::seconds startup_scan_interval_{
      internal::Scanner::startup_interval_s};

  // The wall-clock time when the next startup scan iteration is allowed to run
  std::chrono::steady_clock::time_point next_startup_scan_time_;

  // Flag and cursor for the exhaustive 0x00-0xff scan.
  // fullScanAddress_ iterates from 0 to 255.
  bool full_scan_ = false;
  uint16_t full_scan_address_ = 0;

  // Configuration for the background discovery of known devices.
  bool scan_on_startup_ = false;
  // Number of full discovery iterations performed so far
  uint8_t startup_scan_count_ = 0;
  // Threshold to stop periodic discovery
  uint8_t max_startup_scans_ = internal::Scanner::max_startup_scans;
  // Buffer of commands for the currently active startup scan iteration
  std::queue<Sequence> startup_queue_;

  // Internal helper to add a scan command without locking (caller must hold
  // lock)
  void scanAddressLocked(uint8_t address);
};

}  // namespace ebus