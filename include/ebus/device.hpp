/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ebus/types.hpp"

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

namespace ebus {

/**
 * Device related information
 */
struct DeviceInfo {
  DeviceInfo() = default;

  uint8_t slave_address = 0xff;
  uint8_t manufacturer = 0;
  const char* manufacturer_name = nullptr;
  ByteView unit_id;
  ByteView software_version;
  ByteView hardware_version;

  // Vendor-specific data
  struct VaillantData {
    StaticSequence<28> serial_number;  // Full 28-character serial number
    ByteView product_code;  // 10-digit product identifier (digits 7-16)
  } vaillant;

  // Statistics
  uint32_t frequency = 0;  // Total messages observed from this device

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Returns the manufacturer name associated with the given eBUS ID.
 */
const char* manufacturerName(uint8_t id);

}  // namespace ebus