/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/device.hpp>
#include <string>
#include <vector>

namespace ebus {

/**
 * Represents a device on the eBUS, identified by its slave address and
 * identification data. Provides methods to update its data.  Also provides
 * static methods to generate scan commands for devices. Vendor-specific scan
 * commands are also supported.
 */
class Device {
 public:
  uint8_t getSlave() const;

  void update(const std::vector<uint8_t>& master,
              const std::vector<uint8_t>& slave);

  std::vector<uint8_t> getIdentificationData() const;
  std::vector<uint8_t> getVendorData(uint8_t sub) const;

  DeviceInfo getDeviceInfo() const;

  static const std::vector<uint8_t> createScanCommand(const uint8_t& slave);
  const std::vector<std::vector<uint8_t>> createVendorScanCommands() const;

 private:
  uint8_t slave_ = 0;

  std::vector<uint8_t> vec_070400_;

  std::vector<uint8_t> vec_b5090124_;
  std::vector<uint8_t> vec_b5090125_;
  std::vector<uint8_t> vec_b5090126_;
  std::vector<uint8_t> vec_b5090127_;

  bool isVaillant() const;
  bool isVaillantValid() const;
};

}  // namespace ebus
