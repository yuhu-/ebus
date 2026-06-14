/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ebus/sequence.hpp"
#include "ebus/types.hpp"

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

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
 * Shared properties for all eBUS data types.
 */
struct DataTypeInfoBase {
  DataType dt = DataType::error;
  const char* name = "UNKNOWN";
  uint8_t size = 0;
  bool is_numeric = false;
  bool is_signed = false;
  bool is_float = false;
  bool has_replacement = false;
  uint32_t replacement_value = 0;
};

/**
 * Public metadata for an eBUS data type.
 */
struct DataTypeInfo : DataTypeInfoBase {
  float factor = 1.0f;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * A variant containing any possible decoded eBUS value.
 */
using DataValue =
    std::variant<std::monostate, uint8_t, int8_t, uint16_t, int16_t, uint32_t,
                 int32_t, int64_t, float, std::string>;

/* --- Core Operations --- */

/**
 * Decodes raw eBUS bytes into a high-level DataValue variant.
 * Handles endianness, bit-reversal, sentinels, and fixed-point scaling.
 *
 * @param dt The expected eBUS data type.
 * @param bytes The raw bytes from the bus.
 * @param e The endianness of the source data (usually Endian::little).
 * @return A DataValue containing the decoded value, or nullopt if size/format
 * is invalid.
 */
std::optional<DataValue> decode(DataType dt, ByteView data,
                                Endian e = Endian::little);

/**
 * Performs a shallow validity check on a byte sequence for a specific type.
 * Verifies minimum size and data-specific constraints (e.g., BCD nibble
 * ranges).
 *
 * @param dt The eBUS data type to check against.
 * @param bytes The bytes to validate.
 * @return True if the bytes are plausible for the given type.
 */
bool isValid(DataType dt, ByteView data) noexcept;

/**
 * Encodes a high-level DataValue into a sequence of eBUS-compatible bytes.
 * Handles scaling, range checking, sentinels for nulls, and endianness.
 *
 * @param dt The target eBUS data type.
 * @param value The value to encode.
 * @param e The target endianness (usually Endian::little).
 * @return A Sequence containing the encoded bytes, or an empty sequence if out
 * of range.
 */
Sequence encode(DataType dt, const DataValue& value, Endian e = Endian::little);

/* --- DataValue Utilities --- */

/**
 * Returns a DataValue representing a null state (std::monostate).
 */
DataValue nullValue() noexcept;

/**
 * Returns true if the DataValue is currently in a null state (monostate).
 *
 * @param value The value to check.
 * @return True if null.
 */
bool isNull(const DataValue& value) noexcept;

/**
 * Returns true if the DataValue contains a numeric type (integral or float).
 *
 * @param value The value to check.
 * @return True if the variant holds a numeric type.
 */
bool isNumeric(const DataValue& value) noexcept;

/* --- DataValue Conversion --- */

/**
 * Extracts a numeric value as a float, regardless of underlying variant type.
 * Returns 0.0 for non-numeric types or if the DataValue is null.
 *
 * @param value The DataValue to convert.
 * @return The value as a float.
 */
float asFloat(const DataValue& value) noexcept;

/**
 * Extracts a numeric value as a 64-bit integer, rounding if necessary.
 * Also attempts to parse numeric strings. Returns 0 for non-numeric types.
 *
 * @param value The DataValue to convert.
 * @return The value as an int64_t.
 */
int64_t asInt64(const DataValue& value) noexcept;

/**
 * Returns a reference to the internal string if the variant holds one.
 */
std::string const& asString(const DataValue& value) noexcept;

/**
 * Formats a DataValue into a human-readable string.
 *
 * @param value The DataValue to format.
 * @param unit An optional unit string to append to numeric values.
 */
void toString(std::string& out, const DataValue& value,
              std::string_view unit = "");

/**
 * Formats a DataValue into a human-readable string.
 */
inline std::string toString(const DataValue& value,
                            std::string_view unit = "") {
  std::string res;
  toString(res, value, unit);
  return res;
}

/**
 * Formats a DataValue as a hex string (e.g., "DE AD BE EF").
 */
void toHexString(std::string& out, const DataValue& value,
                 char separator = ' ');

/**
 * Formats a DataValue as a hex string.
 */
inline std::string toHexString(const DataValue& value, char separator = ' ') {
  std::string res;
  toHexString(res, value, separator);
  return res;
}

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
            float val = static_cast<float>(arg);
            return (val >= static_cast<float>(
                               (std::numeric_limits<T>::lowest)()) &&
                    val <= static_cast<float>((std::numeric_limits<T>::max)()))
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

/* --- Metadata & Type Discovery --- */

/**
 * Returns metadata for a specific data type.
 *
 * @param dt The DataType to get metadata for.
 * @return An optional DataTypeInfo struct containing the metadata.
 */
std::optional<DataTypeInfo> getMeta(DataType dt);

/**
 * Returns true if the eBUS DataType represents a numeric value (int, float,
 * fixed-point).
 *
 * @param dt The DataType to check.
 * @return True if the DataType is numeric, false otherwise.
 */
bool isNumeric(DataType dt) noexcept;

/**
 * Infers the most suitable eBUS DataType for a given variant value.
 *
 * @param value The DataValue to infer the type from.
 * @return The inferred DataType, or DataType::error if no suitable type is
 * found.
 */
DataType getDataType(const DataValue& value) noexcept;

/**
 * Returns a list of all eBUS data types supported by the internal engine.
 *
 * @return A vector of supported DataType metadata structures.
 * @note Prefer fetchSupportedDataTypes() to avoid heap allocations.
 */
std::vector<DataTypeInfo> getSupportedDataTypes();

/**
 * @brief Invokes a callback for each supported eBUS data type.
 * Prevents heap allocation of a result vector.
 */
void fetchSupportedDataTypes(std::function<void(const DataTypeInfo&)> callback);

/**
 * @brief Streams a JSON array of all supported eBUS data types to the visitor.
 */
void fetchSupportedDataTypes(const JsonChunkVisitor& visitor,
                             bool pretty = false);

/**
 * Returns the protocol-level byte size of an eBUS DataType.
 *
 * @param data_type The DataType to get metadata for.
 * @return The size in bytes.
 */
constexpr size_t sizeOfDataType(DataType data_type) noexcept {
  if (data_type == DataType::error) return 0;
  return static_cast<size_t>(data_type) >> 8;
}

/**
 * Converts a DataType enum to its string representation (e.g., "UINT16").
 *
 * @param data_type The enum to convert.
 * @return The C-string name.
 */
const char* dataTypeToString(DataType data_type) noexcept;

/**
 * Parses a string representation back into a DataType enum.
 *
 * @param str The string name of the type.
 * @return The DataType enum, or DataType::error if not recognized.
 */
DataType stringToDataType(const char* str);

/**
 * @brief Decodes raw eBUS bytes and streams it as a JSON object to the visitor.
 * @param dt The expected eBUS data type.
 * @param data The raw bytes from the bus.
 * @param pretty Whether to pretty-print the JSON output.
 */
void decodeToJson(const JsonChunkVisitor& visitor, DataType dt, ByteView data,
                  bool pretty = false);

}  // namespace ebus
