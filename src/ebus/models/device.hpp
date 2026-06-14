/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/detail/delegate.hpp>
#include <ebus/device.hpp>
#include <ebus/sequence.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ebus::detail {

/**
 * Represents a device on the eBUS, identified by its slave address and
 * identification data. Provides methods to update its data.  Also provides
 * static methods to generate scan commands for devices. Vendor-specific scan
 * commands are also supported.
 */
class Device {
 public:
  // Lifecycle & Static Factories
  static Sequence createScanCommand(uint8_t slave);

  // Working Methods
  void update(uint8_t slave_addr, ByteView master_view, ByteView slave_view);
  bool getNextPendingVendorCommand(uint16_t& cursor, Sequence& out_cmd) const;
  bool getFirstPendingVendorCommand(Sequence& out_cmd) const;

  // Status/Telemetry
  bool isIdentified() const { return identified_; }
  uint8_t getSlave() const;
  std::vector<uint8_t> getIdentificationData() const;
  std::vector<uint8_t> getVendorData(uint8_t sub) const;
  DeviceInfo getDevice() const;

 private:
  // Internal types
  using ModelSequence = SequenceImpl<detail::SequenceLimits::model_capacity>;

  uint8_t slave_ = 0;
  bool identified_ = false;  // True if 07 04 has been successfully received
  uint32_t message_count_ = 0;

  ModelSequence vec_070400_;

  ModelSequence vec_b5090124_;
  ModelSequence vec_b5090125_;
  ModelSequence vec_b5090126_;
  ModelSequence vec_b5090127_;

  bool isVaillant() const;
  bool isVaillantValid() const;
};

}  // namespace ebus::detail
