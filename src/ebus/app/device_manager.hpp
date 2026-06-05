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
#include <map>
#include <mutex>
#include <set>

#include "core/handler.hpp"
#include "models/device.hpp"

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
  void vendorScanCommands(
      const std::function<void(const Sequence&)>& callback) const;
  void createScanCommands(
      const std::vector<std::string>& addresses,
      const std::function<void(const Sequence&)>& callback) const;

  // Status/Telemetry
  void fetchDeviceInfo(
      const std::function<void(const DeviceInfo&)>& callback) const;
  void getObservedSlaves(std::bitset<256>& observed) const;
  DeviceManagerStatus getStatus() const;

 private:
  uint8_t own_address_ = 0xff;
  BusMonitor* monitor_ = nullptr;
  size_t max_devices_ = DeviceLimits::max_devices;

  mutable std::mutex mutex_;

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
