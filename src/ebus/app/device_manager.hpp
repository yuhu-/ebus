/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <ebus/device.hpp>
#include <ebus/sequence.hpp>
#include <map>
#include <mutex>
#include <set>

#include "core/handler.hpp"
#include "ebus/metrics.hpp"
#include "models/device.hpp"

namespace ebus {

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
  explicit DeviceManager(BusMonitor* monitor = nullptr);

  void setOwnAddress(uint8_t address);

  void update(ByteView master_view, ByteView slave_view);

  void resetDevices();

  std::vector<DeviceInfo> getDeviceInfo() const;

  // TODO what for ???
  std::vector<std::pair<uint8_t, uint32_t>> getMasters() const;
  std::vector<std::pair<uint8_t, uint32_t>> getSlaves() const;

  std::bitset<256> getObservedSlaves() const;
  std::vector<Sequence> vendorScanCommands() const;
  std::vector<Sequence> createScanCommands(
      const std::vector<std::string>& addresses) const;

 private:
  uint8_t own_address_ = 0xff;
  BusMonitor* monitor_ = nullptr;

  mutable std::mutex mutex_;

  std::array<Device, 256> devices_;
  std::bitset<256> identified_devices_{};
};

}  // namespace ebus
