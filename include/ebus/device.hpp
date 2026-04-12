/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>

namespace ebus {

/**
 * Device related information
 */
struct DeviceInfo {
  uint8_t slave_address_ = 0xff;
  uint8_t manufacturer_ = 0;
  std::string manufacturer_name_;
  std::string unit_id_;
  std::string software_version_;
  std::string hardware_version_;

  // Vendor-specific data
  struct VaillantData {
    std::string serial_number_;  // Full 28-character serial number
    std::string product_code_;   // 10-digit product identifier (digits 7-16)
  } vaillant;
};

}  // namespace ebus