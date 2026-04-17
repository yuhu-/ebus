/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ebus/definitions.hpp"
#include "ebus/sequence.hpp"

namespace ebus {

enum class Endian { little, big };

enum class DataType : int32_t {
  auto_detect = 0,
  error = -1,
  // Size 1: 0x01xx
  bcd = 0x0101,
  uint8 = 0x0102,
  int8 = 0x0103,
  data1b = 0x0104,
  data1c = 0x0105,
  char1 = 0x0106,
  hex1 = 0x0107,
  // Size 2: 0x02xx
  uint16 = 0x0201,
  uint16r = 0x0202,
  int16 = 0x0203,
  int16r = 0x0204,
  data2b = 0x0205,
  data2br = 0x0206,
  data2c = 0x0207,
  data2cr = 0x0208,
  char2 = 0x0209,
  hex2 = 0x020a,
  // Size 3: 0x03xx
  char3 = 0x0301,
  hex3 = 0x0302,
  // Size 4: 0x04xx
  uint32 = 0x0401,
  uint32r = 0x0402,
  int32 = 0x0403,
  int32r = 0x0404,
  float4 = 0x0405,
  float4r = 0x0406,
  char4 = 0x0407,
  hex4 = 0x0408,
  // Size 5: 0x05xx
  char5 = 0x0501,
  hex5 = 0x0502,
  // Size 6: 0x06xx
  char6 = 0x0601,
  hex6 = 0x0602,
  // Size 7: 0x07xx
  char7 = 0x0701,
  hex7 = 0x0702,
  // Size 8: 0x08xx
  char8 = 0x0801,
  hex8 = 0x0802
};

/**
 * Public metadata for an eBUS data type.
 */
struct DataTypeInfo {
  DataType dt = DataType::error;
  const char* name = "UNKNOWN";
  uint8_t size = 0;
  bool is_numeric = false;
  bool is_signed = false;
  bool is_float = false;
  double factor = 1.0;
  bool has_replacement = false;
  uint32_t replacement_value = 0;
};

/**
 * A variant containing any possible decoded eBUS value.
 */
using DataValue =
    std::variant<std::monostate, uint8_t, int8_t, uint16_t, int16_t, uint32_t,
                 int32_t, float_t, double_t, std::string>;

/**
 * Runtime decoding dispatcher.
 */
std::optional<DataValue> decode(DataType dt, ByteView bytes,
                                Endian e = Endian::little);

/**
 * Returns true if the provided bytes represent a valid sequence for the
 * specified data type (checking size and internal constraints like BCD).
 */
bool isValid(DataType dt, ByteView bytes) noexcept;

/**
 * Runtime encoding dispatcher.
 */
Sequence encode(DataType dt, const DataValue& value, Endian e = Endian::little);

/**
 * Helpers to extract values from a DataValue without manual variant checking.
 */
bool isNull(const DataValue& value) noexcept;

/**
 * Returns true if the DataValue contains a numeric type (integral or float).
 * Use this to verify compatibility before calling getAs<T>().
 */
bool isNumeric(const DataValue& value) noexcept;

/**
 * Returns a DataValue containing a null state (monostate).
 */
DataValue nullValue() noexcept;

double_t asDouble(const DataValue& value) noexcept;
int64_t asInt64(const DataValue& value) noexcept;

/**
 * Safely casts a DataValue to a numeric type with range checking.
 * Returns std::nullopt if the value is not numeric or falls outside the
 * range of T.
 */
template <typename T>
std::optional<T> getAs(const DataValue& value) noexcept {
  static_assert(std::is_arithmetic_v<T>, "getAs requires a numeric type");
  return std::visit(
      [](auto&& arg) -> std::optional<T> {
        using ArgT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_arithmetic_v<ArgT>) {
          if constexpr (std::is_floating_point_v<T> ||
                        std::is_floating_point_v<ArgT>) {
            double_t val = static_cast<double_t>(arg);
            return (val >= static_cast<double_t>(
                               (std::numeric_limits<T>::lowest)()) &&
                    val <=
                        static_cast<double_t>((std::numeric_limits<T>::max)()))
                       ? std::optional<T>(static_cast<T>(val))
                       : std::nullopt;
          } else {
            int64_t val = static_cast<int64_t>(arg);
            return (val >= static_cast<int64_t>(
                               (std::numeric_limits<T>::lowest)()) &&
                    val <=
                        static_cast<int64_t>((std::numeric_limits<T>::max)()))
                       ? std::optional<T>(static_cast<T>(val))
                       : std::nullopt;
          }
        }
        return std::nullopt;
      },
      value);
}

/**
 * Returns a reference to the internal string if the variant holds one.
 * Returns an empty string otherwise. Use this for raw data access.
 */
std::string const& asString(const DataValue& value) noexcept;

/**
 * Formats a DataValue (especially strings/hex) as a hex string with a custom
 * separator.
 */
std::string toHexString(const DataValue& value, char separator = ' ');

/**
 * Formats any DataValue (numeric or string) into a human-readable string.
 * Handles precision for floats and optional units.
 */
std::string toString(const DataValue& value, std::string_view unit = "");

/**
 * Returns a list of all eBUS data types supported by the internal engine.
 */
std::vector<DataType> getSupportedDataTypes();

/**
 * Infers the most suitable eBUS DataType for a given variant value.
 */
DataType getDataType(const DataValue& value) noexcept;

/**
 * Returns metadata for a specific data type.
 */
std::optional<DataTypeInfo> getMeta(DataType dt);

constexpr size_t sizeOfDataType(DataType data_type) noexcept {
  if (data_type == DataType::error) return 0;
  return static_cast<size_t>(data_type) >> 8;
}

const char* dataTypeToString(DataType data_type) noexcept;
DataType stringToDataType(const char* str);

}  // namespace ebus