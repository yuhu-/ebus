/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Generates scan commands to discover devices on the eBUS. It acts as a state
// machine that, when prompted, provides the next command to be sent. It
// prioritizes manual scans, then deep scans, then full scans, then startup
// scans. supports three modes with a clear priority: Manual Scan > Full Scan >
// Startup Scan.

#pragma once

#include <chrono>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/circular_buffer.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <ebus/static_vector.hpp>
#include <functional>

#include "app/device_manager.hpp"
#include "platform/mutex.hpp"

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
  // Lifecycle & Static Factories
  DeviceScanner(uint8_t address, DeviceManager* device_manager);
  void stop();

  // Configuration
  void setOwnAddress(uint8_t address);
  void setScanOnStartup(bool enable);
  bool isScanOnStartup() const;
  void setMaxStartupScans(uint8_t max);
  void setInitialScanDelay(uint32_t delay_s);
  void setStartupScanInterval(uint32_t interval_s);

  /**
   * @brief Sets a predicate to check if the system is too busy to perform
   * background scans. Postpones discovery during high bus activity or bridge
   * sessions.
   */
  void setBusyPredicate(Delegate<bool()> pred);

  // Working Methods
  void initFullScan(bool enable);
  /** @brief Triggers a deep scan for all currently observed (but unknown)
   * devices. */
  bool scanObservedDevices();
  bool scanAddress(uint8_t address);
  bool scanAddresses(const std::vector<uint8_t>& addresses);
  Sequence nextCommand();
  /**
   * @brief Informs the scanner about the result of a previously issued scan
   * command. This is used for backoff and state management.
   */
  void onScanResult(uint8_t address, bool success);
  void resetPeakMetrics();

  // Status/Telemetry
  bool isFullScan() const;
  bool isScanning() const;
  DeviceScannerStatus fetchStatus() const;

 private:
  DeviceManager* device_manager_ = nullptr;
  uint8_t own_address_ = ebus::RuntimeConfig{}.address;

  // Protects all internal state across threads (Controller and Scheduler)
  mutable platform::Mutex mutex_;

  // Predicate to check if the system is busy, used for throttling background
  // scans.
  Delegate<bool()> is_busy_;

  // Bitset to track addresses that need a "deep scan" (07 04 + vendor
  // specific).
  std::bitset<256> pending_deep_scans_;
  // Bitset to track addresses that have recently failed a scan and are on
  // cooldown.
  std::bitset<256> failed_scans_;
  // Bitset to track addresses that persistently fail or loop and are
  // quarantined.
  std::bitset<256> quarantined_scans_;
  // The wall-clock time when the failed_scans_ bitset was last cleared.
  Clock::time_point last_failure_reset_time_ = Clock::time_point::min();
  // The duration for which a failed scan is put on cooldown (batch).
  std::chrono::seconds scan_cooldown_duration_ = std::chrono::seconds(60);

  // Timing configuration for the discovery/startup phase.
  std::chrono::seconds initial_scan_delay_{
      ebus::RuntimeConfig{}.device.initial_delay_s};
  std::chrono::seconds startup_scan_interval_{
      ebus::RuntimeConfig{}.device.startup_interval_s};

  // The wall-clock time when the next startup scan iteration is allowed to run
  Clock::time_point next_startup_scan_time_;

  // State for the exhaustive 0x00-0xFF "surface scan".
  bool full_scan_ = false;
  uint16_t full_scan_address_ = 0;

  // State for the background discovery of observed devices on startup.
  bool scan_on_startup_ = false;
  // Number of full discovery iterations performed so far
  uint8_t startup_scan_count_ = 0;
  // Threshold to stop periodic discovery
  uint8_t max_startup_scans_ = ebus::RuntimeConfig{}.device.max_startup_scans;

  // Generator state for the current deep scan (manual, observed, or startup).
  // This tracks the address and the progress through its vendor commands.
  uint16_t current_deep_scan_address_ = 256;
  uint16_t current_deep_scan_vendor_cursor_ = 0;

  // State for the startup scan logic.
  bool startup_iteration_active_ = false;
  uint16_t startup_observed_cursor_ =
      0;  // Cursor for finding next observed device
  uint16_t startup_current_device_addr_ =
      256;  // The device currently being processed
  bool startup_current_device_id_scan_issued_ =
      false;  // True if ID scan for current device is done
  uint16_t startup_current_device_vendor_cursor_ =
      0;  // Cursor for vendor commands of current device

  // Tracks identification progress attempts to prevent loops on faulty devices.
  std::array<uint8_t, 256> scan_attempt_counters_{};
  uint32_t failure_resets_ = 0;
  Clock::time_point last_scan_attempt_ = Clock::time_point::min();

  bool scanAddressInternal(uint8_t address);
};

}  // namespace ebus::detail