/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/device.hpp>
#include <ebus/sequence.hpp>
#include <map>
#include <mutex>
#include <set>

#include "core/handler.hpp"
#include "models/device.hpp"

namespace ebus {

/**
 * Manages devices on the eBUS, identified by their slave address and
 * identification data. Collects data from eBUS messages to identify devices and
 * their manufacturers. Provides methods to generate scan commands for
 * discovered devices. Also tracks master and slave addresses observed on the
 * bus.
 */
class DeviceManager {
 public:
  void setOwnAddress(uint8_t address);

  void update(ByteView master, ByteView slave);

  void resetAddresses();

  std::vector<DeviceInfo> getDeviceInfo() const;

  std::map<uint8_t, uint32_t> getMasters() const;
  std::map<uint8_t, uint32_t> getSlaves() const;

  std::set<uint8_t> getObservedSlaves() const;
  std::vector<Sequence> vendorScanCommands() const;
  std::vector<Sequence> createScanCommands(
      const std::vector<std::string>& addresses) const;

 private:
  uint8_t own_address_ = 0xff;

  mutable std::mutex mutex_;

  std::map<uint8_t, Device> devices_;
  std::map<uint8_t, uint32_t> masters_;
  std::map<uint8_t, uint32_t> slaves_;
};

}  // namespace ebus
