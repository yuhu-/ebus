/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/device.hpp>
#include <ebus/sequence.hpp>
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

  void update(ByteView master_view, ByteView slave_view);

  std::vector<uint8_t> getIdentificationData() const;
  std::vector<uint8_t> getVendorData(uint8_t sub) const;

  DeviceInfo getDeviceInfo() const;

  static Sequence createScanCommand(uint8_t slave);
  std::vector<Sequence> createVendorScanCommands() const;

 private:
  uint8_t slave_ = 0;

  Sequence vec_070400_;

  Sequence vec_b5090124_;
  Sequence vec_b5090125_;
  Sequence vec_b5090126_;
  Sequence vec_b5090127_;

  bool isVaillant() const;
  bool isVaillantValid() const;
};

}  // namespace ebus
