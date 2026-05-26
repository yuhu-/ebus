/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

/**
 * Device related information
 */
struct DeviceInfo {
  uint8_t slave_address = 0xff;
  uint8_t manufacturer = 0;
  std::string manufacturer_name;
  std::string unit_id;
  std::string software_version;
  std::string hardware_version;

  // Vendor-specific data
  struct VaillantData {
    std::string serial_number;  // Full 28-character serial number
    std::string product_code;   // 10-digit product identifier (digits 7-16)
  } vaillant;

  // Statistics
  uint32_t frequency = 0;  // Total messages observed from this device

  void toJson(std::string& json) const;
};

/**
 * Returns the manufacturer name associated with the given eBUS ID.
 */
const char* manufacturerName(uint8_t id);

}  // namespace ebus