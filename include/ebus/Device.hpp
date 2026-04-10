/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>

namespace ebus {

struct DeviceInfo {
  uint8_t slave = 0xff;
  uint8_t manufacturer = 0;
  std::string manufacturerName;
  std::string unitID;
  std::string softwareVersion;
  std::string hardwareVersion;

  // Vendor-specific data
  struct VaillantData {
    std::string serial;       // Full 28-character serial number
    std::string productCode;  // 10-digit product identifier (digits 7-16)
  } vaillant;
};

}  // namespace ebus