/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Manages devices on the eBUS, identified by their slave address and
// identification data. Collects data from eBUS messages to identify devices and
// their manufacturers. Provides methods to generate scan commands for
// discovered devices. Also tracks master and slave addresses observed on the
// bus.

#pragma once

#include <cstdint>
#include <ebus/Device.hpp>
#include <map>
#include <mutex>
#include <set>

#include "Core/Handler.hpp"
#include "Models/Device.hpp"

namespace ebus {

class DeviceManager {
 public:
  void setOwnAddress(uint8_t address);

  void update(const std::vector<uint8_t>& master,
              const std::vector<uint8_t>& slave);

  void resetAddresses();

  std::vector<DeviceInfo> getDeviceInfo() const;

  std::map<uint8_t, uint32_t> getMasters() const;
  std::map<uint8_t, uint32_t> getSlaves() const;

  std::set<uint8_t> getObservedSlaves() const;
  const std::vector<std::vector<uint8_t>> vendorScanCommands() const;
  const std::vector<std::vector<uint8_t>> createScanCommands(
      const std::vector<std::string>& addresses) const;

 private:
  uint8_t ownAddress_ = 0xff;

  mutable std::mutex mutex_;

  std::map<uint8_t, Device> devices_;
  std::map<uint8_t, uint32_t> masters_;
  std::map<uint8_t, uint32_t> slaves_;
};

}  // namespace ebus
