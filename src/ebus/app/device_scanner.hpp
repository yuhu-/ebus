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
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <mutex>
#include <vector>

#include "app/device_manager.hpp"
#include "core/handler.hpp"
#include "utils/static_vector.hpp"

namespace ebus::detail {

/**
 * The DeviceScanner is responsible for generating eBUS scan commands to
 * discover devices on the bus. It operates as a state machine that prioritizes
 * three scanning modes: Manual Scan (initiated by explicit requests), Full Scan
 * (exhaustive 0x00-0xFF), and Startup Scan (periodic discovery of known
 * devices). The scanner maintains internal queues for manual and startup scans.
 */
class DeviceScanner {
 public:
  // Lifecycle
  DeviceScanner(uint8_t address, DeviceManager* device_manager);
  void stop();

  // Configuration
  void setOwnAddress(uint8_t address);
  void setScanOnStartup(bool enable);
  bool isScanOnStartup() const;
  void setMaxStartupScans(uint8_t max);
  void setInitialScanDelay(uint32_t delay_s);
  void setStartupScanInterval(uint32_t interval_s);

  // Working Methods
  void initFullScan(bool enable);
  bool scanObservedDevices();
  bool scanAddress(uint8_t address);
  bool scanAddresses(const std::vector<uint8_t>& addresses);
  Sequence nextCommand();
  void resetPeakMetrics();

  // Status/Telemetry
  bool isFullScan() const;
  bool isScanning() const;
  DeviceScannerStatus getStatus() const;

 private:
  DeviceManager* device_manager_ = nullptr;
  uint8_t own_address_ = ebus::RuntimeConfig{}.address;

  // Protects all internal state across threads (Controller and Scheduler)
  mutable std::mutex mutex_;

  // Commands explicitly requested via scanAddress() or scanObservedDevices().
  // These are always returned first.
  StaticVector<Sequence, DeviceLimits::max_manual_queue> manual_queue_;

  // Timing configuration for the discovery/startup phase.
  std::chrono::seconds initial_scan_delay_{
      ebus::RuntimeConfig{}.device.initial_delay_s};
  std::chrono::seconds startup_scan_interval_{
      ebus::RuntimeConfig{}.device.startup_interval_s};

  // The wall-clock time when the next startup scan iteration is allowed to run
  Clock::time_point next_startup_scan_time_;

  // Flag and cursor for the exhaustive 0x00-0xFF scan.
  // fullScanAddress_ iterates from 0 to 255.
  bool full_scan_ = false;
  uint16_t full_scan_address_ = 0;

  // Configuration for the background discovery of known devices.
  bool scan_on_startup_ = false;
  // Number of full discovery iterations performed so far
  uint8_t startup_scan_count_ = 0;
  // Threshold to stop periodic discovery
  uint8_t max_startup_scans_ = ebus::RuntimeConfig{}.device.max_startup_scans;
  // Buffer of commands for the currently active startup scan iteration
  StaticVector<Sequence, DeviceLimits::max_startup_queue> startup_queue_;

  size_t max_manual_queue_size_ = 0;
  size_t max_startup_queue_size_ = 0;

  // Internal helper to add a scan command without locking (caller must hold
  // lock)
  bool scanAddressLocked(uint8_t address);
};

}  // namespace ebus::detail