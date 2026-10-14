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
 * Renders raw bytes as an eBUS character string (ebusd STR parity):
 * terminates at the first 0x00, maps bytes below 0x20 to space,
 * non-printable bytes (>= 0x7F) to '?', everything else literal.
 * Used for CHAR payloads and device identification strings alike.
 */
std::string renderEbusString(ByteView data);

/**
 * Decodes raw eBUS bytes into a high-level DataValue variant.
 * Handles endianness, bit-reversal, sentinels, and fixed-point scaling.
 * use_sentinel=false returns the raw numeric value instead (presentation
 * choice; wire decoding is identical).
 */
std::optional<DataValue> decode(DataType dt, ByteView data,
                                Endian e = Endian::little,
                                bool use_sentinel = true);

/** Shallow plausibility check: minimum size plus BCD nibble ranges. */
bool isValid(DataType dt, ByteView data) noexcept;

/**
 * Encodes a DataValue into eBUS bytes. Handles scaling, range checking,
 * null-to-sentinel mapping, and endianness. Empty sequence = out of range
 * (or null with no replacement defined).
 */
Sequence encode(DataType dt, const DataValue& value, Endian e = Endian::little);

/* --- DataValue Utilities --- */

/** Null state (std::monostate). */
DataValue nullValue() noexcept;

/** True if the variant holds monostate. */
bool isNull(const DataValue& value) noexcept;

/** True for integral/float alternatives. */
bool isNumeric(const DataValue& value) noexcept;

/* --- DataValue Conversion --- */

/** Numeric value as float (fixed-point aware); 0.0 for strings/null. */
float asFloat(const DataValue& value) noexcept;

/** Numeric value as int64 (fixed-point aware, rounded). Parses strings. */
int64_t asInt64(const DataValue& value) noexcept;

/** Reference to the held string; empty static on mismatch (no throw). */
std::string const& asString(const DataValue& value) noexcept;

/** Human-readable rendering with optional appended unit. */
void toString(std::string& out, const DataValue& value,
              std::string_view unit = "");

/** Owning variant of the above. */
inline std::string toString(const DataValue& value,
                            std::string_view unit = "") {
  std::string res;
  toString(res, value, unit);
  return res;
}

/** Hex rendering ("DE AD BE EF"), custom separator. */
void toHexString(std::string& out, const DataValue& value,
                 char separator = ' ');

/** Owning variant of the above. */
inline std::string toHexString(const DataValue& value, char separator = ' ') {
  std::string res;
  toHexString(res, value, separator);
  return res;
}

/** Range-checked numeric cast; nullopt for non-numeric or out of range. */
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

/** Metadata for dt; nullopt if unknown. */
std::optional<DataTypeInfo> getMeta(DataType dt);

/** True for int/float/fixed-point types. */
bool isNumeric(DataType dt) noexcept;

/** Best-fit DataType for a variant value; error if none fits. */
DataType getDataType(const DataValue& value) noexcept;

/**
 * All supported types. Heap-allocating; prefer the callback/visitor
 * variants on constrained targets.
 */
std::vector<DataTypeInfo> getSupportedDataTypes();

/** Allocation-free iteration over supported types. */
void fetchSupportedDataTypes(std::function<void(const DataTypeInfo&)> callback);

/** Streams the supported types as a JSON array. */
void fetchSupportedDataTypes(const JsonChunkVisitor& visitor,
                             bool pretty = false);

/**
 * Protocol byte size. Encoded in the enum itself (high byte), so this is
 * O(1) with no table lookup; error maps to 0.
 */
constexpr size_t sizeOfDataType(DataType data_type) noexcept {
  if (data_type == DataType::error) return 0;
  return static_cast<size_t>(data_type) >> 8;
}

/** Enum to name ("UINT16"); "UNKNOWN" for unrecognized values. */
const char* dataTypeToString(DataType data_type) noexcept;

/** Name to enum (exact uppercase match); error if unrecognized. */
DataType stringToDataType(const char* str);

/** Decodes dt and streams it as a JSON object. Respects use_sentinel=true. */
void decodeToJson(const JsonChunkVisitor& visitor, DataType dt, ByteView data,
                  bool pretty = false);

}  // namespace ebus
