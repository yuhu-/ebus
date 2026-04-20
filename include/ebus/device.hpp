/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
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
};

/**
 * Serializes DeviceInfo to a JSON object string.
 */
inline std::string toJson(const DeviceInfo& info) {
  std::ostringstream oss;
  oss << "{"
      << "\"slave_address\":" << static_cast<int>(info.slave_address) << ","
      << "\"manufacturer\":" << static_cast<int>(info.manufacturer) << ","
      << "\"manufacturer_name\":\"" << info.manufacturer_name << "\","
      << "\"unit_id\":\"" << info.unit_id << "\","
      << "\"software_version\":\"" << info.software_version << "\","
      << "\"hardware_version\":\"" << info.hardware_version << "\"";

  if (!info.vaillant.serial_number.empty()) {
    oss << ",\"vaillant\":{"
        << "\"serial_number\":\"" << info.vaillant.serial_number << "\","
        << "\"product_code\":\"" << info.vaillant.product_code << "\""
        << "}";
  }

  oss << "}";
  return oss.str();
}

/**
 * Serializes a vector of DeviceInfo to a JSON array string.
 */
inline std::string toJson(const std::vector<DeviceInfo>& devices) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < devices.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(devices[i]);
  }
  oss << "]";
  return oss.str();
}

}  // namespace ebus