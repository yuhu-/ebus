/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <ebus/data_types.hpp>
#include <ebus/detail/json_writer.hpp>  // For detail::JsonWriter
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <limits>
#include <string_view>
#include <utility>

namespace ebus {

namespace {
struct Scale {
  int32_t num;
  int32_t den;
};

struct Meta : DataTypeInfoBase {
  bool reversed = false;
  Scale scale = {1, 1};
};

// Table MUST be sorted alphabetically by 'name' for binary search in
// stringToDataType
// clang-format off
constexpr Meta kMetaTable[] = {
  {{DataType::bcd, "BCD", 1, true, false, false, true, 0xff}, false, {1, 1}},
  {{DataType::char1, "CHAR1", 1, false, false, false, true, 0xff}, false, {1, 1}},
  {{DataType::char2, "CHAR2", 2, false, false, false, true, 0xffff}, false, {1, 1}},
  {{DataType::char3, "CHAR3", 3, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char4, "CHAR4", 4, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char5, "CHAR5", 5, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char6, "CHAR6", 6, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char7, "CHAR7", 7, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char8, "CHAR8", 8, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::data1b, "DATA1B", 1, true, true, false, true, 0x80}, false, {1, 1}},
  {{DataType::data1c, "DATA1C", 1, true, false, false, true, 0xff}, false, {1, 2}},
  {{DataType::data2b, "DATA2B", 2, true, true, false, true, 0x8000}, false, {1, 256}},
  {{DataType::data2br, "DATA2BR", 2, true, true, false, true, 0x8000}, true, {1, 256}},
  {{DataType::data2c, "DATA2C", 2, true, true, false, true, 0x8000}, false, {1, 16}},
  {{DataType::data2cr, "DATA2CR", 2, true, true, false, true, 0x8000}, true, {1, 16}},
  {{DataType::float4, "FLOAT4", 4, true, false, true, false, 0}, false, {1, 1}},
  {{DataType::float4r, "FLOAT4R", 4, true, false, true, false, 0}, true, {1, 1}},
  {{DataType::hex1, "HEX1", 1, false, false, false, true, 0xff}, false, {1, 1}},
  {{DataType::hex2, "HEX2", 2, false, false, false, true, 0xffff}, false, {1, 1}},
  {{DataType::hex3, "HEX3", 3, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex4, "HEX4", 4, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex5, "HEX5", 5, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex6, "HEX6", 6, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex7, "HEX7", 7, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex8, "HEX8", 8, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::int16, "INT16", 2, true, true, false, true, 0x8000}, false, {1, 1}},
  {{DataType::int16r, "INT16R", 2, true, true, false, true, 0x8000}, true, {1, 1}},
  {{DataType::int32, "INT32", 4, true, true, false, true, 0x80000000}, false, {1, 1}},
  {{DataType::int32r, "INT32R", 4, true, true, false, true, 0x80000000}, true, {1, 1}},
  {{DataType::int8, "INT8", 1, true, true, false, true, 0x80}, false, {1, 1}},
  {{DataType::uint16, "UINT16", 2, true, false, false, true, 0xffff}, false, {1, 1}},
  {{DataType::uint16r, "UINT16R", 2, true, false, false, true, 0xffff}, true, {1, 1}},
  {{DataType::uint32, "UINT32", 4, true, false, false, true, 0xffffffff}, false, {1, 1}},
  {{DataType::uint32r, "UINT32R", 4, true, false, false, true, 0xffffffff}, true, {1, 1}},
  {{DataType::uint8, "UINT8", 1, true, false, false, true, 0xff}, false, {1, 1}},
};
// clang-format on

/**
 * Maps a DataType enum value to the kMetaLookup table index.
 */
constexpr int getDataTypeIndex(DataType dt) {
  const uint32_t val = static_cast<uint32_t>(dt);
  return static_cast<int>(((val >> 8) << 4) | (val & 0xff));
}

/**
 * Internal helper to generate the lookup table at compile time.
 */
constexpr std::array<const Meta*, 144> generateMetaLookup() {
  std::array<const Meta*, 144> arr{};
  for (const auto& m : kMetaTable) {
    arr[getDataTypeIndex(m.dt)] = &m;
  }
  return arr;
}

static constexpr auto kMetaLookup = generateMetaLookup();

const Meta* metaFor(DataType dt) {
  if (dt == DataType::error || dt == DataType::auto_detect) return nullptr;

  const int idx = getDataTypeIndex(dt);
  if (idx < 0 || static_cast<size_t>(idx) >= kMetaLookup.size()) return nullptr;
  return kMetaLookup[idx];
}

/**
 * Visitor for DataValue variant to collapse integral logic into two
 * non-templated overloads (int64/uint64), minimizing monomorphization bloat in
 * Flash.
 */
struct JsonValueVisitor {
  detail::JsonWriter& writer;

  void operator()(std::monostate) const { writer.write("null"); }

  void operator()(const std::string& s) const {
    writer.write("\"");
    writer.writeEscaped(s);
    writer.write("\"");
  }

  void operator()(int64_t val) const {
    // This handles the generic int64_t in the variant, which represents
    // fixed-point scaled values in our protocol.
    char buffer[64];
    char* end_ptr = formatFloat(static_cast<float>(val) / FIXED_POINT_SCALE, 4,
                                buffer, sizeof(buffer),
                                detail::FormattingLimits::float_lower_threshold,
                                detail::FormattingLimits::float_upper_threshold);
    writer.write(std::string_view(buffer, end_ptr - buffer));
  }

  void operator()(float val) const {
    char buffer[64];
    char* end_ptr = formatFloat(val, 4, buffer, sizeof(buffer),
                                detail::FormattingLimits::float_lower_threshold,
                                detail::FormattingLimits::float_upper_threshold);
    writer.write(std::string_view(buffer, end_ptr - buffer));
  }

  // Handler for all other integral types in the variant
  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  void operator()(T val) const {
    append_int(static_cast<int64_t>(val));
  }

 private:
  void append_int(int64_t val) const {
    char buffer[24];
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), val);
    if (ec == std::errc{})
      writer.write(std::string_view(buffer, ptr - buffer));
    else
      writer.write("null");
  }

  void append_int(uint64_t val) const {
    char buffer[24];
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), val);
    if (ec == std::errc{})
      writer.write(std::string_view(buffer, ptr - buffer));
    else
      writer.write("null");
  }
};

template <typename IntType>
constexpr IntType readInt(ebus::ByteView data, bool flip) {
  IntType val = 0;
  // Spec 2.4.3: Low-byte first. By using shifts we reconstruct the integer
  // in a host-endian independent way.
  for (size_t i = 0; i < sizeof(IntType); ++i) {
    val |= (static_cast<IntType>(data[i]) << (8 * i));
  }
  return flip ? swapEndian(val) : val;
}

template <typename IntType>
void writeInt(ebus::Sequence& s, IntType val, bool flip) {
  const IntType to_write = flip ? swapEndian(val) : val;
  // Spec 2.4.3: Low-byte first.
  for (size_t i = 0; i < sizeof(IntType); ++i)
    s.pushBack(static_cast<uint8_t>((to_write >> (8 * i)) & 0xff), false);
}

/**
 * Scaled value using integer math to preserve precision and minimize FPU usage.
 */
template <typename T>
constexpr int64_t integerScale(T val, int32_t num, int32_t den) {
  if (den == 0) return 0;
#ifdef __SIZEOF_INT128__
  // Use __int128_t for intermediate calculation to prevent overflow
  __int128_t intermediate_numerator = static_cast<__int128_t>(val) * num;
  __int128_t half =
      static_cast<__int128_t>(den) / 2;  // Keep original rounding logic
  if (intermediate_numerator >= 0) {
    return static_cast<int64_t>((intermediate_numerator + half) / den);
  }
  return static_cast<int64_t>((intermediate_numerator - half) / den);
#else
  // Fallback for compilers without __int128_t (e.g., MSVC).
  // This version is susceptible to overflow if (val * num) exceeds int64_t max.
  int64_t numerator = static_cast<int64_t>(val) * num;
  int64_t half = (den > 0) ? (den / 2) : (-den / 2);
  if (numerator >= 0) {
    return (numerator + half) / den;
  }
  return (numerator - half) / den;
#endif
}

template <typename T>
constexpr bool inRange(int64_t val, bool is_signed) {
  if (is_signed) {
    return val >=
               std::numeric_limits<typename std::make_signed<T>::type>::min() &&
           val <=
               std::numeric_limits<typename std::make_signed<T>::type>::max();
  }
  return val >= 0 &&
         static_cast<uint64_t>(val) <=
             std::numeric_limits<typename std::make_unsigned<T>::type>::max();
}

bool validateRange(int64_t val, uint8_t size, bool is_signed) {
  switch (size) {
    case 1:
      return inRange<int8_t>(val, is_signed);
    case 2:
      return inRange<int16_t>(val, is_signed);
    case 4:
      return inRange<int32_t>(val, is_signed);
    default:
      return true;
  }
}

}  // namespace

/* --- Core Operations --- */

std::optional<DataValue> decode(DataType dt, ByteView data, Endian e) {
  const Meta* m = metaFor(dt);
  if (!m || data.size() < m->size) return std::nullopt;

  const bool flip = m->reversed ? (e == Endian::little) : (e == Endian::big);

  if (!m->is_numeric)
    return std::string(reinterpret_cast<const char*>(data.data()), m->size);

  // Check for Replacement Values (Sentinels)
  uint32_t bit_pattern = 0;
  switch (m->size) {
    case 1:
      bit_pattern = data[0];
      break;
    case 2:
      bit_pattern = readInt<uint16_t>(data, flip);
      break;
    case 4:
      bit_pattern = readInt<uint32_t>(data, flip);
      break;
    default:
      break;
  }

  if (!m->is_float && m->has_replacement &&
      bit_pattern == m->replacement_value) {
    return nullValue();
  }

  if (dt == DataType::bcd) {
    uint8_t val = data[0];
    if ((val & 0x0f) > 9 || ((val >> 4) & 0x0f) > 9) return std::nullopt;
    return static_cast<uint8_t>((val >> 4) * 10 + (val & 0x0f));
  }

  // Read the raw integer value from bytes
  int64_t raw_val = 0;
  if (m->size == 1)
    raw_val = m->is_signed ? static_cast<int8_t>(data[0]) : data[0];
  else if (m->size == 2)
    raw_val = m->is_signed
                  ? static_cast<int16_t>(readInt<uint16_t>(data, flip))
                  : static_cast<int64_t>(readInt<uint16_t>(data, flip));
  else if (m->size == 4)
    raw_val = m->is_signed
                  ? static_cast<int32_t>(readInt<uint32_t>(data, flip))
                  : static_cast<int64_t>(readInt<uint32_t>(data, flip));

  if (m->is_float) {
    uint32_t raw = readInt<uint32_t>(data, flip);
    float f;
    std::memcpy(&f, &raw, 4);
    return f;
  }

  // Handle scaled integer types (DATA1C, DATA2B, etc.)
  if (m->scale.num != 1 || m->scale.den != 1) {
    // Store as int64_t fixed-point: value * FIXED_POINT_SCALE
    return static_cast<int64_t>(
        (static_cast<int64_t>(raw_val) * m->scale.num * FIXED_POINT_SCALE) /
        m->scale.den);
  }

  // Handle unscaled integer types
  if (m->size == 1)
    return m->is_signed ? DataValue(static_cast<int8_t>(raw_val))
                        : DataValue(static_cast<uint8_t>(raw_val));
  if (m->size == 2)
    return m->is_signed ? DataValue(static_cast<int16_t>(raw_val))
                        : DataValue(static_cast<uint16_t>(raw_val));
  return m->is_signed ? DataValue(static_cast<int32_t>(raw_val))
                      : DataValue(static_cast<uint32_t>(raw_val));
}

bool isValid(DataType dt, ByteView data) noexcept {
  const Meta* m = metaFor(dt);
  if (!m || data.size() < m->size) return false;

  if (dt == DataType::bcd) {
    uint8_t val = data[0];
    // Sentinel 0xFF is valid as a "null" result, not a protocol error.
    if (val == 0xff) return true;
    return (val & 0x0f) <= 9 && ((val >> 4) & 0x0f) <= 9;
  }
  return true;
}

Sequence encode(DataType dt, const DataValue& value, Endian e) {
  if (dt == DataType::auto_detect) {
    dt = getDataType(value);
  }

  const Meta* m = metaFor(dt);
  Sequence s;
  if (!m) return s;
  s.reserve(m->size);

  const bool flip = m->reversed ? (e == Endian::little) : (e == Endian::big);

  // Handle numeric null sentinel encoding
  if (m->is_numeric && isNull(value)) {
    if (!m->has_replacement) return Sequence{};  // no sentinel defined
    if (m->size == 1) {
      s.pushBack(static_cast<uint8_t>(m->replacement_value), false);
    } else if (m->size == 2) {
      writeInt<uint16_t>(s, static_cast<uint16_t>(m->replacement_value), flip);
    } else if (m->size == 4) {
      writeInt<uint32_t>(s, static_cast<uint32_t>(m->replacement_value), flip);
    } else {
      // Generic: write low bytes of replacement_value in little-first order
      for (uint8_t i = 0; i < m->size; ++i)
        s.pushBack(
            static_cast<uint8_t>((m->replacement_value >> (8 * i)) & 0xff),
            false);
    }
    return s;
  }

  // Handle Strings
  if (!m->is_numeric) {
    if (auto* str = std::get_if<std::string>(&value)) {
      s.assign(ByteView(reinterpret_cast<const uint8_t*>(str->data()),
                        std::min(str->size(), static_cast<size_t>(m->size))),
               false);
      while (s.size() < m->size) s.pushBack(0, false);
    }
    return s;
  }

  // Handle BCD
  if (dt == DataType::bcd) {
    int64_t v = asInt64(value);
    if (v < 0 || v > 99) return s;  // Returns empty sequence to indicate error

    s.pushBack(
        ((static_cast<uint8_t>(v) / 10) << 4) | (static_cast<uint8_t>(v) % 10),
        false);
    return s;
  }

  // Handle Floats
  if (m->is_float) {
    float f = asFloat(value);
    uint32_t raw;
    std::memcpy(&raw, &f, 4);
    writeInt<uint32_t>(s, raw, flip);
    return s;
  }

  // Handle Integers and Fixed-Point
  int64_t raw_int = 0;
  if (std::holds_alternative<float>(value)) {
    raw_int = static_cast<int64_t>(
        std::round(asFloat(value) * m->scale.den / m->scale.num));
  } else if (std::holds_alternative<int64_t>(value)) {  // Fixed-point int64_t
    // Convert from fixed-point to float, then unscale.
    // Direct integer math for unscaling can overflow int64_t for large values.
    raw_int = static_cast<int64_t>(std::round(
        (static_cast<float>(std::get<int64_t>(value)) / FIXED_POINT_SCALE) *
        m->scale.den / m->scale.num));
  } else {
    raw_int = integerScale(asInt64(value), m->scale.den, m->scale.num);
  }

  // Prevent bit-truncation by checking ranges
  if (!validateRange(raw_int, m->size, m->is_signed)) return s;

  if (m->size == 1)
    s.pushBack(static_cast<uint8_t>(raw_int), false);
  else if (m->size == 2)
    writeInt<uint16_t>(s, static_cast<uint16_t>(raw_int), flip);
  else if (m->size == 4)
    writeInt<uint32_t>(s, static_cast<uint32_t>(raw_int), flip);

  return s;
}

/* --- DataValue Utilities --- */

DataValue nullValue() noexcept { return DataValue(std::monostate{}); }

bool isNull(const DataValue& value) noexcept {
  return std::holds_alternative<std::monostate>(value);
}

bool isNumeric(const DataValue& value) noexcept {
  return std::visit(
      [](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        return std::is_arithmetic_v<T>;
      },
      value);
}

/* --- DataValue Conversion --- */

float asFloat(const DataValue& value) noexcept {
  return std::visit(
      [](auto&& arg) -> float {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {  // Fixed-point int64_t
          return static_cast<float>(arg) / FIXED_POINT_SCALE;
        }
        if constexpr (std::is_arithmetic_v<T>) return static_cast<float>(arg);
        return 0.0f;
      },
      value);
}

int64_t asInt64(const DataValue& value) noexcept {
  return std::visit(
      [](auto&& arg) -> int64_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {  // Fixed-point int64_t
          // Convert from fixed-point to integer with rounding (FPU-free)
          return (arg + FIXED_POINT_SCALE / 2) / FIXED_POINT_SCALE;
        }
        if constexpr (std::is_integral_v<T>) return static_cast<int64_t>(arg);
        if constexpr (std::is_floating_point_v<T>)
          return static_cast<int64_t>(std::round(arg));
        if constexpr (std::is_same_v<T, std::string>) {
          if (arg.empty()) return 0;
          std::string_view sv = arg;
          int base = 10;
          // Handle optional 0x prefix for hex strings
          if (sv.size() >= 2 && sv[0] == '0' &&
              (sv[1] == 'x' || sv[1] == 'X')) {
            sv.remove_prefix(2);
            base = 16;
          } else {
            // Default to hex for eBUS string fields (which usually hold hex
            // pairs)
            base = 16;
          }
          int64_t result = 0;
          auto [ptr, ec] =
              std::from_chars(sv.data(), sv.data() + sv.size(), result, base);
          if (ec == std::errc{}) return result;
        }
        return 0;
      },
      value);
}

std::string const& asString(const DataValue& value) noexcept {
  static const std::string empty;
  if (const std::string* s = std::get_if<std::string>(&value)) {
    return *s;
  }
  return empty;
}

// New struct for the visitor
struct ToStringValueVisitor {
  std::string operator()(std::monostate) const { return "null"; }
  std::string operator()(const std::string& s) const { return s; }

  std::string operator()(int64_t val) const {
    char buffer[64];
    char* end = formatFloat(static_cast<float>(val) / FIXED_POINT_SCALE, 2,
                            buffer, sizeof(buffer),
                            detail::FormattingLimits::float_lower_threshold,
                            detail::FormattingLimits::float_upper_threshold);
    return std::string(buffer, end - buffer);
  }

  std::string operator()(float val) const {
    char buffer[64];
    char* end = formatFloat(val, 2, buffer, sizeof(buffer),
                            detail::FormattingLimits::float_lower_threshold,
                            detail::FormattingLimits::float_upper_threshold);
    return std::string(buffer, end - buffer);
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  std::string operator()(T val) const {
    return std::to_string(val);
  }
};

std::string toString(const DataValue& value, std::string_view unit) {
  std::string s = std::visit(ToStringValueVisitor{}, value);
  if (!unit.empty() && s != "null" && !s.empty()) {
    s += " ";
    s += unit;
  }
  return s;
}

std::string toHexString(const DataValue& value, char separator) {
  if (const std::string* s = std::get_if<std::string>(&value)) {
    if (s->empty()) return "";
    std::string res;
    res.reserve(s->size() + (separator ? s->size() / 2 : 0));
    for (size_t i = 0; i < s->size(); ++i) {
      if (i > 0 && i % 2 == 0 && separator != 0) res += separator;
      res += (*s)[i];
    }
    return res;
  }
  return toString(value);
}

/* --- Metadata & Type Discovery --- */

std::optional<DataTypeInfo> getMeta(DataType dt) {
  const Meta* m = metaFor(dt);
  if (!m) return std::nullopt;

  DataTypeInfo info;
  static_cast<DataTypeInfoBase&>(info) = *m;
  info.factor = static_cast<float>(m->scale.num) / m->scale.den;
  return info;
}

bool isNumeric(DataType dt) noexcept {
  const Meta* m = metaFor(dt);
  return m && m->is_numeric;
}

DataType getDataType(const DataValue& value) noexcept {
  return std::visit(
      [](auto&& arg) -> DataType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, uint8_t>) return DataType::uint8;
        if constexpr (std::is_same_v<T, int8_t>) return DataType::int8;
        if constexpr (std::is_same_v<T, uint16_t>) return DataType::uint16;
        if constexpr (std::is_same_v<T, int16_t>) return DataType::int16;
        if constexpr (std::is_same_v<T, uint32_t>) return DataType::uint32;
        if constexpr (std::is_same_v<T, int32_t>) return DataType::int32;
        if constexpr (std::is_same_v<T, int64_t>)
          return DataType::error;  // Cannot infer specific scaled type from
                                   // generic int64_t
        if constexpr (std::is_floating_point_v<T>) return DataType::float4;
        if constexpr (std::is_same_v<T, std::string>) return DataType::char8;
        return DataType::error;
      },
      value);
}

std::vector<DataTypeInfo> getSupportedDataTypes() {
  std::vector<DataTypeInfo> types;
  types.reserve(sizeof(kMetaTable) / sizeof(kMetaTable[0]));
  for (const auto& m : kMetaTable) {
    DataTypeInfo info;
    static_cast<DataTypeInfoBase&>(info) = m;
    info.factor = static_cast<float>(m.scale.num) / m.scale.den;
    types.push_back(info);
  }
  return types;
}

const char* dataTypeToString(DataType data_type) noexcept {
  auto m = getMeta(data_type);
  return m ? m->name : "UNKNOWN";
}

DataType stringToDataType(const char* str) {
  auto it = std::lower_bound(
      std::begin(kMetaTable), std::end(kMetaTable), str,
      [](const Meta& m, const char* s) { return std::strcmp(m.name, s) < 0; });
  if (it != std::end(kMetaTable) && std::strcmp(it->name, str) == 0)
    return it->dt;
  return DataType::error;
}

void DataTypeInfo::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("type", static_cast<int64_t>(dt));
  writer.writeField("name", name);
  writer.writeField("size", static_cast<uint64_t>(size));
  writer.writeField("is_numeric", is_numeric);
  writer.writeField("is_signed", is_signed);
  writer.writeField("is_float", is_float);
  writer.writeField("has_replacement", has_replacement);
  writer.writeField("replacement_value",
                    static_cast<uint64_t>(replacement_value));
  writer.writeFieldFloat("factor", factor, 4);
  writer.endObject();
}

std::string getSupportedDataTypesJson() {
  std::string json;
  json.reserve(8192);
  getSupportedDataTypesJson([&json](std::string_view s) { json.append(s); });
  return json;
}

void getSupportedDataTypesJson(const JsonChunkVisitor& visitor) {
  detail::JsonWriter writer(visitor);
  writer.startArray();
  bool first = true;
  const auto types = getSupportedDataTypes();
  for (const auto& t : types) {
    if (!first) writer.write(",");
    t.toJson(visitor);
    first = false;
  }
  writer.endArray();
}

std::string decodeToJson(DataType dt, ByteView data) {
  std::string json;
  json.reserve(512);
  decodeToJson([&json](std::string_view s) { json.append(s); }, dt, data);
  return json;
}

void decodeToJson(const JsonChunkVisitor& visitor, DataType dt, ByteView data) {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  auto meta_opt = getMeta(dt);
  if (!meta_opt) {
    writer.writeField("error", "Invalid DataType");
  } else {
    const auto& meta = *meta_opt;
    writer.writeField("type", static_cast<int64_t>(meta.dt));
    writer.writeField("name", meta.name);
    writer.writeHexField("raw_hex", data);
    auto decoded = decode(dt, data);
    writer.appendKey("value");
    if (decoded)
      std::visit(JsonValueVisitor{writer}, *decoded);
    else
      writer.write("null");
    writer.writeFieldFloat("factor", meta.factor, 4);
  }
  writer.endObject();
}

}  // namespace ebus
