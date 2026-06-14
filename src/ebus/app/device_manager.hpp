/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <ebus/device.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <ebus/status.hpp>
#include <functional>

#include "models/device.hpp"
#include "platform/mutex.hpp"

namespace ebus::detail {

class BusMonitor;

/**
 * Manages devices on the eBUS, identified by their slave address and
 * identification data. Collects data from eBUS messages to identify devices and
 * their manufacturers. Provides methods to generate scan commands for
 * discovered devices. Also tracks master and slave addresses observed on the
 * bus.
 */
class DeviceManager {
 public:
  // Lifecycle
  explicit DeviceManager(BusMonitor* monitor = nullptr);

  // Configuration
  void setOwnAddress(uint8_t address);

  // Working Methods
  void update(ByteView master_view, ByteView slave_view);

  /**
   * @brief Finds the next slave address that has been observed but not
   * necessarily identified.
   * @param start The address to start searching from (inclusive).
   * @return The next observed slave address, or 256 if none found.
   */
  uint16_t findNextObservedSlave(uint8_t start) const;

  /**
   * @brief Finds the next pending vendor command for a specific device.
   * @param device_addr The address of the device to query.
   * @param cursor An in/out parameter representing the current position in the
   * vendor command list for this device.
   * @param out_cmd Reference to store the generated command.
   * @return true if a pending vendor command was found, false otherwise.
   */
  bool getNextPendingVendorCommandForDevice(uint8_t device_addr,
                                            uint16_t& cursor,
                                            Sequence& out_cmd) const;

  /**
   * @brief Finds the next identified device starting from start_addr that has
   * pending (missing) vendor identification commands.
   * @param start_addr The address to start searching from (inclusive).
   * @param out_cmd Reference to store the generated command.
   * @return The address of the device found, or 256 if none found.
   */
  uint16_t findNextPendingVendorCommand(uint16_t start_addr,
                                        Sequence& out_cmd) const;

  // Status/Telemetry
  /**
   * @brief Returns true if the device at the given address has been identified
   * (i.e., its 07 04 data has been successfully received).
   */
  bool isIdentified(uint8_t addr) const;

  /**
   * @brief Returns true if the device at the given address is identified
   * and still needs vendor-specific data to be fully profiled.
   */
  bool needsDeepScan(uint8_t addr) const;

  void getObservedSlaves(std::bitset<256>& observed) const;

  void fetchDevices(
      const std::function<void(const DeviceInfo&)>& callback) const;

  DeviceManagerStatus fetchStatus() const;

 private:
  uint8_t own_address_ = 0xff;
  BusMonitor* monitor_ = nullptr;
  size_t max_devices_ = DeviceLimits::max_devices;

  mutable platform::Mutex mutex_;

  std::array<Device, DeviceLimits::max_devices> device_pool_;
  std::array<int16_t, 256>
      address_map_;  // Maps slave address to pool index, -1 if unused
  size_t pool_usage_ = 0;

  std::bitset<256> identified_devices_{};  // Tracks if address has an
                                           // associated Device entry
  std::bitset<256> masters_{};
  std::bitset<256> slaves_{};
};

}  // namespace ebus::detail
