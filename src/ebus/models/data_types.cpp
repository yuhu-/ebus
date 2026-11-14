/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ebus/data_types.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/detail/protocol_limits.hpp>
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

// Semantic string formats for date/time/PIN types: wire layout and output
// format differ per type, so meta entries carry the format instead of
// decoding through the generic numeric path.
enum class Fmt : uint8_t {
  none = 0,
  date_bcd4,
  date_bcd3,
  date_bcd4z,
  date_bin3,
  date_bin4,
  date_day2,
  datetime_min4,
  time_bcd3r,
  time_bcd2r,
  time_bin3r,
  time_bin3,
  time_bin2r,
  time_bin2,
  time_min2,
  time_trunc10,
  time_trunc30,
  time_trunc15,
  pin2,
  tem_p,
};

struct Meta : DataTypeInfoBase {
  bool reversed = false;
  Scale scale = {1, 1};
  Fmt fmt = Fmt::none;
};

// Table MUST be sorted alphabetically by 'name' for binary search in
// stringToDataType
// clang-format off
constexpr Meta meta_table[] = {
  {{DataType::bcd16, "BCD16", 2, true, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}},
  {{DataType::bcd24, "BCD24", 3, true, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}},
  {{DataType::bcd32, "BCD32", 4, true, false, false, true, detail::DataTypeLimits::sentinel_32}, false, {1, 1}},
  {{DataType::bcd8, "BCD8", 1, true, false, false, true, detail::DataTypeLimits::null_sentinel}, false, {1, 1}},
  {{DataType::char1, "CHAR1", 1, false, false, false, true, detail::DataTypeLimits::sentinel_8}, false, {1, 1}},
  {{DataType::char10, "CHAR10", 10, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char11, "CHAR11", 11, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char12, "CHAR12", 12, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char13, "CHAR13", 13, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char14, "CHAR14", 14, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char15, "CHAR15", 15, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char16, "CHAR16", 16, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char2, "CHAR2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}},
  {{DataType::char3, "CHAR3", 3, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char4, "CHAR4", 4, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char5, "CHAR5", 5, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char6, "CHAR6", 6, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char7, "CHAR7", 7, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char8, "CHAR8", 8, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::char9, "CHAR9", 9, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::data1b, "DATA1B", 1, true, true, false, true, detail::DataTypeLimits::sentinel_s8}, false, {1, 1}},
  {{DataType::data1c, "DATA1C", 1, true, false, false, true, detail::DataTypeLimits::sentinel_8}, false, {1, 2}},
  {{DataType::data2b, "DATA2B", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, false, {1, 256}},
  {{DataType::data2br, "DATA2BR", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, true, {1, 256}},
  {{DataType::data2c, "DATA2C", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, false, {1, 16}},
  {{DataType::data2cr, "DATA2CR", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, true, {1, 16}},
  {{DataType::datetime_min4, "DATETIME_MIN4", 4, false, false, false, false, 0}, false, {1, 1}, Fmt::datetime_min4},
  {{DataType::date_bcd3, "DATE_BCD3", 3, false, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}, Fmt::date_bcd3},
  {{DataType::date_bcd4, "DATE_BCD4", 4, false, false, false, true, detail::DataTypeLimits::sentinel_32}, false, {1, 1}, Fmt::date_bcd4},
  {{DataType::date_bcd4z, "DATE_BCD4Z", 4, false, false, false, true, detail::DataTypeLimits::sentinel_32}, false, {1, 1}, Fmt::date_bcd4z},
  {{DataType::date_bin3, "DATE_BIN3", 3, false, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}, Fmt::date_bin3},
  {{DataType::date_bin4, "DATE_BIN4", 4, false, false, false, true, detail::DataTypeLimits::sentinel_32}, false, {1, 1}, Fmt::date_bin4},
  {{DataType::date_day2, "DATE_DAY2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::date_day2},
  {{DataType::float32, "FLOAT32", 4, true, false, true, false, 0}, false, {1, 1}},
  {{DataType::float32r, "FLOAT32R", 4, true, false, true, false, 0}, true, {1, 1}},
  {{DataType::hex1, "HEX1", 1, false, false, false, true, detail::DataTypeLimits::sentinel_8}, false, {1, 1}},
  {{DataType::hex10, "HEX10", 10, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex11, "HEX11", 11, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex12, "HEX12", 12, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex13, "HEX13", 13, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex14, "HEX14", 14, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex15, "HEX15", 15, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex16, "HEX16", 16, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex2, "HEX2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}},
  {{DataType::hex3, "HEX3", 3, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex4, "HEX4", 4, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex5, "HEX5", 5, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex6, "HEX6", 6, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex7, "HEX7", 7, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex8, "HEX8", 8, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::hex9, "HEX9", 9, false, false, false, false, 0}, false, {1, 1}},
  {{DataType::int16, "INT16", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, false, {1, 1}},
  {{DataType::int16r, "INT16R", 2, true, true, false, true, detail::DataTypeLimits::sentinel_s16}, true, {1, 1}},
  {{DataType::int24, "INT24", 3, true, true, false, true, detail::DataTypeLimits::sentinel_s24}, false, {1, 1}},
  {{DataType::int24r, "INT24R", 3, true, true, false, true, detail::DataTypeLimits::sentinel_s24}, true, {1, 1}},
  {{DataType::int32, "INT32", 4, true, true, false, true, detail::DataTypeLimits::sentinel_s32}, false, {1, 1}},
  {{DataType::int32r, "INT32R", 4, true, true, false, true, detail::DataTypeLimits::sentinel_s32}, true, {1, 1}},
  {{DataType::int8, "INT8", 1, true, true, false, true, detail::DataTypeLimits::sentinel_s8}, false, {1, 1}},
  {{DataType::pin2, "PIN2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::pin2},
  {{DataType::tem_p, "TEM_P", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::tem_p},
  {{DataType::time_bcd2r, "TIME_BCD2R", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::time_bcd2r},
  {{DataType::time_bcd3r, "TIME_BCD3R", 3, false, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}, Fmt::time_bcd3r},
  {{DataType::time_bin2, "TIME_BIN2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::time_bin2},
  {{DataType::time_bin2r, "TIME_BIN2R", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::time_bin2r},
  {{DataType::time_bin3, "TIME_BIN3", 3, false, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}, Fmt::time_bin3},
  {{DataType::time_bin3r, "TIME_BIN3R", 3, false, false, false, true, 0x636363}, false, {1, 1}, Fmt::time_bin3r},
  {{DataType::time_min2, "TIME_MIN2", 2, false, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}, Fmt::time_min2},
  {{DataType::time_trunc10, "TIME_TRUNC10", 1, false, false, false, true, 0x90}, false, {1, 1}, Fmt::time_trunc10},
  {{DataType::time_trunc15, "TIME_TRUNC15", 1, false, false, false, true, 0}, false, {1, 1}, Fmt::time_trunc15},
  {{DataType::time_trunc30, "TIME_TRUNC30", 1, false, false, false, true, 0}, false, {1, 1}, Fmt::time_trunc30},
  {{DataType::uint16, "UINT16", 2, true, false, false, true, detail::DataTypeLimits::sentinel_16}, false, {1, 1}},
  {{DataType::uint16r, "UINT16R", 2, true, false, false, true, detail::DataTypeLimits::sentinel_16}, true, {1, 1}},
  {{DataType::uint24, "UINT24", 3, true, false, false, true, detail::DataTypeLimits::sentinel_24}, false, {1, 1}},
  {{DataType::uint24r, "UINT24R", 3, true, false, false, true, detail::DataTypeLimits::sentinel_24}, true, {1, 1}},
  {{DataType::uint32, "UINT32", 4, true, false, false, true, detail::DataTypeLimits::sentinel_32}, false, {1, 1}},
  {{DataType::uint32r, "UINT32R", 4, true, false, false, true, detail::DataTypeLimits::sentinel_32}, true, {1, 1}},
  {{DataType::uint8, "UINT8", 1, true, false, false, true, detail::DataTypeLimits::sentinel_8}, false, {1, 1}},
};
// clang-format on

constexpr int getDataTypeIndex(DataType dt) {
  const uint32_t val = static_cast<uint32_t>(dt);
  // Slot within a size class must stay < 32 (encoded in the low 5 bits);
  // larger slots would alias into the next size class.
  return static_cast<int>(((val >> 8) << 5) | (val & 0x1f));
}

// Lookup table generated at compile time (no startup cost, no heap).
constexpr std::array<const Meta*, 544> generateMetaLookup() {
  std::array<const Meta*, 544> arr{};
  for (const auto& m : meta_table) {
    arr[getDataTypeIndex(m.dt)] = &m;
  }
  return arr;
}

static constexpr auto meta_lookup = generateMetaLookup();

const Meta* metaFor(DataType dt) {
  if (dt == DataType::error || dt == DataType::auto_detect) return nullptr;

  const int idx = getDataTypeIndex(dt);
  if (idx < 0 || static_cast<size_t>(idx) >= meta_lookup.size()) return nullptr;
  return meta_lookup[idx];
}

// Collapses integral logic into two non-templated overloads (int64/uint64)
// to minimize monomorphization bloat in Flash.
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
    char buffer[detail::FormattingLimits::iso8601_buffer_size +
                10];  // Sized for float expansion
    const char* end_ptr = formatFloat(
        static_cast<float>(val) / detail::FixedPointLimits::fixed_point_scale,
        detail::FormattingLimits::detailed_precision, buffer, sizeof(buffer),
        detail::FormattingLimits::float_lower_threshold,
        detail::FormattingLimits::float_upper_threshold);
    writer.write(std::string_view(buffer, end_ptr - buffer));
  }

  void operator()(float val) const {
    char buffer[detail::FormattingLimits::iso8601_buffer_size + 10];
    const char* end_ptr = formatFloat(
        val, detail::FormattingLimits::detailed_precision, buffer,
        sizeof(buffer), detail::FormattingLimits::float_lower_threshold,
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
    s.push_back(static_cast<uint8_t>((to_write >> (8 * i)) & 0xff), false);
}

// No native 3-byte integer type, so the24-bit accessors assemble the
// value byte-wise (low byte first, spec 2.4.3); flip reverses the three
// bytes for the R variants.
constexpr uint32_t readInt24(ByteView data, bool flip) {
  uint32_t val = static_cast<uint32_t>(data[0]) |
                 (static_cast<uint32_t>(data[1]) << 8) |
                 (static_cast<uint32_t>(data[2]) << 16);
  if (flip) val = ((val & 0xff) << 16) | (val & 0xff00) | ((val >> 16) & 0xff);
  return val;
}

void writeInt24(Sequence& s, uint32_t val, bool flip) {
  val &= 0xffffff;
  if (flip) val = ((val & 0xff) << 16) | (val & 0xff00) | ((val >> 16) & 0xff);
  s.push_back(static_cast<uint8_t>(val & 0xff), false);
  s.push_back(static_cast<uint8_t>((val >> 8) & 0xff), false);
  s.push_back(static_cast<uint8_t>((val >> 16) & 0xff), false);
}

// Integer math to preserve precision and minimize FPU usage.
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
    case 3:
      if (is_signed) return val >= -8388608 && val <= 8388607;
      return val >= 0 && val <= 0xffffff;
    case 4:
      return inRange<int32_t>(val, is_signed);
    default:
      return true;
  }
}

// --- Date/time/PIN string types ---
// Date wire layout: DD MM [WW] YY — the weekday byte exists only in the
// 4-byte variants and is skipped on read, computed on write. Component-time
// layouts are byte-reversed (hour in the last byte; the forward HTI/HTM
// variants are not yet implemented). Output: dd.MM.yyyy, HH:mm:ss / HH:mm,
// PIN as zero-padded 4-digit string. Replacement is per byte (default 0xff,
// VTI 0x63, TTM 0x90); a replacement byte or a zero day/month renders the
// whole field null — documented divergence from partial-null strings.

bool bcdByte(uint8_t raw, uint8_t* out) {
  if ((raw >> 4) > 9 || (raw & 0x0f) > 9) return false;
  *out = static_cast<uint8_t>((raw >> 4) * 10 + (raw & 0x0f));
  return true;
}

uint8_t bcdPack(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

struct DateTimeParts {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int value = 0;  // PIN, TEM_P (grp * 1000 + num)
};

enum class ParseState : uint8_t { invalid, is_null, ok };

// datetime_min4 counts minutes from 01.01.2009 (MJD 54832) and accepts at
// most 31.12.2099 23:59 (0x02DA4E1F); beyond that the wire value is
// invalid.
constexpr int kDatetimeEpochMjd = 54832;
constexpr uint32_t kDatetimeMaxMinutes = 0x02DA4E1Fu;

// Gregorian date from an MJD day number. The year step uses floor() so
// MJDs before 1900-03-01 (01.01.1900 = 15020) decode to the correct day;
// truncation toward zero would shift those by two days and break the
// day2 roundtrip.
bool mjdToDate(int mjd, DateTimeParts* out) {
  int y = static_cast<int>(std::floor((mjd - 15078.2) / 365.25));
  int m = static_cast<int>((mjd - 14956.1 - static_cast<int>(y * 365.25)) /
                           30.6001);
  int day = mjd - 14956 - static_cast<int>(y * 365.25) -
            static_cast<int>(m * 30.6001);
  m--;
  if (m >= 13) {
    y++;
    m -= 12;
  }
  if (day < 1 || day > 31 || m < 1 || m > 12) return false;
  out->day = day;
  out->month = m;
  out->year = y + 1900;
  return true;
}

// Inverse of mjdToDate for a full year (valid1900..2099).
int dateToMjd(int day, int month, int year) {
  const int y = year - 1900;
  const int l = month <= 2 ? 1 : 0;
  return 14956 + day + static_cast<int>((y - l) * 365.25) +
         static_cast<int>((month + 1 + l * 12) * 30.6001);
}

ParseState parseWire(const Meta& meta, ByteView d, bool use_sentinel,
                     DateTimeParts* out) {
  const uint8_t repl = static_cast<uint8_t>(meta.replacement_value & 0xff);
  // TEM_P nulls on the full 16-bit sentinel only: a lone 0xff byte is
  // legal data (e.g. wire 0x0fff = "31-127").
  if (use_sentinel && meta.has_replacement && meta.fmt != Fmt::tem_p) {
    for (size_t i = 0; i < d.size(); ++i) {
      if (d[i] == repl) return ParseState::is_null;
    }
  }

  switch (meta.fmt) {
    case Fmt::date_bcd4:
    case Fmt::date_bcd3:
    case Fmt::date_bcd4z:
    case Fmt::date_bin3:
    case Fmt::date_bin4: {
      const bool bcd = meta.fmt == Fmt::date_bcd4 ||
                       meta.fmt == Fmt::date_bcd3 ||
                       meta.fmt == Fmt::date_bcd4z;
      uint8_t day, month, year;
      if (bcd) {
        // The weekday byte (index 2) is deliberately not decoded or
        // validated: the reference reader skips it as well. 3-byte types
        // carry no weekday; the year always lands on the last index.
        if (!bcdByte(d[0], &day) || !bcdByte(d[1], &month) ||
            !bcdByte(d[d.size() - 1], &year)) {
          return ParseState::invalid;
        }
      } else {
        day = d[0];
        month = d[1];
        year = d[d.size() - 1];
      }
      // A zero day/month is the wire null marker (!REZ), not an error.
      if (day == 0 || month == 0) return ParseState::is_null;
      if (day > 31 || month > 12 || year > 99) return ParseState::invalid;
      out->day = day;
      out->month = month;
      out->year = 2000 + year;
      return ParseState::ok;
    }
    case Fmt::date_day2: {
      // Days since 01.01.1900 (REZ: zero is a real date, no null marker).
      const unsigned days =
          static_cast<unsigned>(d[0]) | (static_cast<unsigned>(d[1]) << 8);
      if (!mjdToDate(static_cast<int>(days) + 15020, out)) {
        return ParseState::invalid;
      }
      return ParseState::ok;
    }
    case Fmt::datetime_min4: {
      // Minutes since 01.01.2009 (LE); no wire replacement (REQ).
      const uint32_t minutes = static_cast<uint32_t>(d[0]) |
                               (static_cast<uint32_t>(d[1]) << 8) |
                               (static_cast<uint32_t>(d[2]) << 16) |
                               (static_cast<uint32_t>(d[3]) << 24);
      if (minutes > kDatetimeMaxMinutes) return ParseState::invalid;
      const int mjd = static_cast<int>(minutes / (24 * 60)) + kDatetimeEpochMjd;
      if (!mjdToDate(mjd, out)) return ParseState::invalid;
      const unsigned rem = minutes % (24 * 60);
      out->hour = static_cast<int>(rem / 60);
      out->minute = static_cast<int>(rem % 60);
      return ParseState::ok;
    }
    case Fmt::time_min2: {
      const unsigned minutes =
          static_cast<unsigned>(d[0]) | (static_cast<unsigned>(d[1]) << 8);
      if (minutes > 24 * 60) return ParseState::invalid;
      out->hour = static_cast<int>(minutes / 60);
      out->minute = static_cast<int>(minutes % 60);
      return ParseState::ok;
    }
    case Fmt::time_trunc10:
    case Fmt::time_trunc30:
    case Fmt::time_trunc15: {
      // 1-byte step encoding: minutes div step + hour * (60/step); the
      // narrower formats keep fewer bits (10min/8bit, 30min/6bit,
      // 15min/7bit).
      int step = 10;
      int bits = 8;
      if (meta.fmt == Fmt::time_trunc30) {
        step = 30;
        bits = 6;
      } else if (meta.fmt == Fmt::time_trunc15) {
        step = 15;
        bits = 7;
      }
      const int value = d[0] & ((1 << bits) - 1);
      const int hours = value / (60 / step);
      const int minutes = (value % (60 / step)) * step;
      if (hours > 24 || (hours == 24 && minutes > 0)) {
        return ParseState::invalid;
      }
      out->hour = hours;
      out->minute = minutes;
      return ParseState::ok;
    }
    case Fmt::time_bcd3r:
    case Fmt::time_bcd2r:
    case Fmt::time_bin3r:
    case Fmt::time_bin2r: {
      const bool bcd =
          meta.fmt == Fmt::time_bcd3r || meta.fmt == Fmt::time_bcd2r;
      const size_t len = d.size();
      const size_t hi = len - 1;  // hour in the last byte (REV layout)
      const size_t mi = len - 2;
      uint8_t hour, minute, second = 0;
      if (bcd) {
        if (!bcdByte(d[hi], &hour) || !bcdByte(d[mi], &minute)) {
          return ParseState::invalid;
        }
        if (len == 3 && !bcdByte(d[0], &second)) return ParseState::invalid;
      } else {
        hour = d[hi];
        minute = d[mi];
        if (len == 3) second = d[0];
      }
      if (hour > 24 || minute > 59 || (len == 3 && second > 59)) {
        return ParseState::invalid;
      }
      if (hour == 24 && (minute > 0 || (len == 3 && second > 0))) {
        return ParseState::invalid;
      }
      out->hour = hour;
      out->minute = minute;
      out->second = second;
      return ParseState::ok;
    }
    case Fmt::time_bin3:
    case Fmt::time_bin2: {
      // Forward layout: hour first; the 3-byte type carries seconds.
      const size_t len = d.size();
      uint8_t hour = d[0], minute = d[1], second = 0;
      if (len == 3) second = d[2];
      if (hour > 24 || minute > 59 || (len == 3 && second > 59)) {
        return ParseState::invalid;
      }
      if (hour == 24 && (minute > 0 || (len == 3 && second > 0))) {
        return ParseState::invalid;
      }
      out->hour = hour;
      out->minute = minute;
      out->second = second;
      return ParseState::ok;
    }
    case Fmt::pin2: {
      uint8_t high, low;
      if (!bcdByte(d[0], &high) || !bcdByte(d[1], &low)) {
        return ParseState::invalid;
      }
      out->value = high * 100 + low;
      return ParseState::ok;
    }
    case Fmt::tem_p: {
      // Slave-side parameter id: xxxx GGGG GNNN NNNN (LE word); the read
      // masks keep grp <= 31 and num <= 127, so only the sentinel
      // 0xffff can be null.
      const unsigned v =
          static_cast<unsigned>(d[0]) | (static_cast<unsigned>(d[1]) << 8);
      if (use_sentinel && meta.has_replacement && v == meta.replacement_value) {
        return ParseState::is_null;
      }
      const unsigned grp = (v >> 7) & 0x1f;
      const unsigned num = v & 0x7f;
      out->value = static_cast<int>(grp * 1000 + num);
      return ParseState::ok;
    }
    case Fmt::none:
    default:
      return ParseState::invalid;
  }
}

std::optional<DataValue> decodeString(const Meta& meta, ByteView d,
                                      bool use_sentinel) {
  DateTimeParts p;
  switch (parseWire(meta, d, use_sentinel, &p)) {
    case ParseState::invalid:
      return std::nullopt;
    case ParseState::is_null:
      return nullValue();
    case ParseState::ok:
      break;
  }
  char buf[24];
  switch (meta.fmt) {
    case Fmt::date_bcd4:
    case Fmt::date_bcd3:
    case Fmt::date_bcd4z:
    case Fmt::date_bin3:
    case Fmt::date_bin4:
    case Fmt::date_day2:
      std::snprintf(buf, sizeof(buf), "%02d.%02d.%04d", p.day, p.month, p.year);
      break;
    case Fmt::datetime_min4:
      std::snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d", p.day,
                    p.month, p.year, p.hour, p.minute);
      break;
    case Fmt::time_bcd3r:
    case Fmt::time_bin3r:
    case Fmt::time_bin3:
      std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", p.hour, p.minute,
                    p.second);
      break;
    case Fmt::pin2:
      std::snprintf(buf, sizeof(buf), "%04d", p.value);
      break;
    case Fmt::tem_p:
      std::snprintf(buf, sizeof(buf), "%02d-%03d", p.value / 1000,
                    p.value % 1000);
      break;
    default:
      std::snprintf(buf, sizeof(buf), "%02d:%02d", p.hour, p.minute);
      break;
  }
  return std::string(buf);
}

bool parseIntToken(const std::string& s, size_t begin, size_t end, int* out) {
  if (begin >= end) return false;
  int value = 0;
  auto [ptr, ec] = std::from_chars(s.data() + begin, s.data() + end, value);
  if (ec != std::errc{} || ptr != s.data() + end) return false;
  *out = value;
  return true;
}

// Strict "dd.MM.yyyy" parse; unpadded components accepted. Year accepts
// 0..99 and 2000..2099 always, plus full years
// >= min_full_year below 2000 when the caller opts in (day counts);
// *out is the full year.
bool parseDateString(const std::string& s, int* day, int* month, int* year,
                     int min_full_year = 2000) {
  const size_t p1 = s.find('.');
  if (p1 == std::string::npos) return false;
  const size_t p2 = s.find('.', p1 + 1);
  if (p2 == std::string::npos) return false;
  if (s.find('.', p2 + 1) != std::string::npos) return false;
  int d, mo, ye;
  if (!parseIntToken(s, 0, p1, &d) || !parseIntToken(s, p1 + 1, p2, &mo) ||
      !parseIntToken(s, p2 + 1, s.size(), &ye)) {
    return false;
  }
  if (ye >= 2000 && ye <= 2099) {
    // full year, keep as-is
  } else if (ye >= 0 && ye <= 99) {
    ye += 2000;
  } else if (ye >= min_full_year && ye < 2000) {
    // full year inside the caller's allowed window (e.g. 1900..1999)
  } else {
    return false;
  }
  if (d < 1 || d > 31 || mo < 1 || mo > 12) return false;
  *day = d;
  *month = mo;
  *year = ye;
  return true;
}

// "HH:mm" or "HH:mm:ss"; the component count must match with_seconds.
bool parseTimeString(const std::string& s, bool with_seconds, int* hour,
                     int* minute, int* second = nullptr) {
  const size_t p1 = s.find(':');
  if (p1 == std::string::npos) return false;
  const size_t p2 = with_seconds ? s.find(':', p1 + 1) : std::string::npos;
  int h, mi, se = 0;
  if (with_seconds) {
    if (p2 == std::string::npos) return false;
    if (s.find(':', p2 + 1) != std::string::npos) return false;
    if (!parseIntToken(s, 0, p1, &h) || !parseIntToken(s, p1 + 1, p2, &mi) ||
        !parseIntToken(s, p2 + 1, s.size(), &se)) {
      return false;
    }
    if (se < 0 || se > 59) return false;
  } else {
    if (s.find(':', p1 + 1) != std::string::npos) return false;
    if (!parseIntToken(s, 0, p1, &h) ||
        !parseIntToken(s, p1 + 1, s.size(), &mi)) {
      return false;
    }
  }
  if (h < 0 || h > 24 || mi < 0 || mi > 59) return false;
  if (h == 24 && (mi > 0 || se > 0)) return false;
  *hour = h;
  *minute = mi;
  if (second) *second = se;
  return true;
}

bool parsePinString(const std::string& s, int* out) {
  if (s.empty() || s.size() > 4) return false;
  int value = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || ptr != s.data() + s.size()) return false;
  if (value < 0 || value > 9999) return false;
  *out = value;
  return true;
}

void pushReplacement(Sequence& s, const Meta& meta) {
  for (uint8_t i = 0; i < meta.size; ++i) {
    s.push_back(
        static_cast<uint8_t>((meta.replacement_value >> (8 * i)) & 0xff),
        false);
  }
}

Sequence encodeString(const Meta& meta, const DataValue& value) {
  Sequence s;
  s.reserve(meta.size);

  // Null encodes to the wire replacement; types without one (REQ) have no
  // representable null and reject the write.
  if (isNull(value)) {
    if (!meta.has_replacement) return Sequence{};
    pushReplacement(s, meta);
    return s;
  }
  const std::string* str = std::get_if<std::string>(&value);
  if (!str) return s;
  // The "null" token maps to the wire replacement on write.
  if (*str == "null") {
    if (!meta.has_replacement) return Sequence{};
    pushReplacement(s, meta);
    return s;
  }

  switch (meta.fmt) {
    case Fmt::date_bcd4:
    case Fmt::date_bcd3:
    case Fmt::date_bcd4z:
    case Fmt::date_bin3:
    case Fmt::date_bin4: {
      int day, month, year;
      if (!parseDateString(*str, &day, &month, &year)) return Sequence{};
      const bool bcd = meta.fmt == Fmt::date_bcd4 ||
                       meta.fmt == Fmt::date_bcd3 ||
                       meta.fmt == Fmt::date_bcd4z;
      const bool has_weekday = meta.size == 4;
      uint8_t weekday = 0;
      if (has_weekday) {
        // Weekday byte computed from the date (MJD math).
        const int mjd = dateToMjd(day, month, year);
        const int days_since_sunday = (mjd + 3) % 7;  // Sun=0
        if (meta.fmt == Fmt::date_bcd4z) {
          // SPE variant: Mon=0x00 .. Sun=0x06.
          weekday = static_cast<uint8_t>((6 + days_since_sunday) % 7);
        } else {
          weekday = static_cast<uint8_t>(
              days_since_sunday == 0 ? 7 : days_since_sunday);  // Mon=1..Sun=7
        }
      }
      if (bcd) {
        s.push_back(bcdPack(static_cast<uint8_t>(day)), false);
        s.push_back(bcdPack(static_cast<uint8_t>(month)), false);
        if (has_weekday) s.push_back(bcdPack(weekday), false);
        s.push_back(bcdPack(static_cast<uint8_t>(year % 100)), false);
        return s;
      }
      s.push_back(static_cast<uint8_t>(day), false);
      s.push_back(static_cast<uint8_t>(month), false);
      if (has_weekday) s.push_back(weekday, false);
      s.push_back(static_cast<uint8_t>(year % 100), false);
      return s;
    }
    case Fmt::date_day2: {
      int day, month, year;
      if (!parseDateString(*str, &day, &month, &year, 1900)) {
        return Sequence{};
      }
      const int days = dateToMjd(day, month, year) - 15020;
      if (days < 0 || days > 0xffff) return Sequence{};
      s.push_back(static_cast<uint8_t>(days & 0xff), false);
      s.push_back(static_cast<uint8_t>((days >> 8) & 0xff), false);
      return s;
    }
    case Fmt::datetime_min4: {
      const size_t sp = str->find(' ');
      if (sp == std::string::npos) return Sequence{};
      int day, month, year, hour, minute;
      if (!parseDateString(str->substr(0, sp), &day, &month, &year) ||
          !parseTimeString(str->substr(sp + 1), false, &hour, &minute)) {
        return Sequence{};
      }
      const int mjd = dateToMjd(day, month, year);
      if (mjd < kDatetimeEpochMjd) return Sequence{};
      const uint32_t minutes =
          static_cast<uint32_t>(mjd - kDatetimeEpochMjd) * 24u * 60u +
          static_cast<uint32_t>(hour) * 60u + static_cast<uint32_t>(minute);
      if (minutes > kDatetimeMaxMinutes) return Sequence{};
      s.push_back(static_cast<uint8_t>(minutes & 0xff), false);
      s.push_back(static_cast<uint8_t>((minutes >> 8) & 0xff), false);
      s.push_back(static_cast<uint8_t>((minutes >> 16) & 0xff), false);
      s.push_back(static_cast<uint8_t>((minutes >> 24) & 0xff), false);
      return s;
    }
    case Fmt::time_bcd3r:
    case Fmt::time_bin3r: {
      int hour, minute, second;
      if (!parseTimeString(*str, true, &hour, &minute, &second)) {
        return Sequence{};
      }
      // REV layout: the least significant component goes into byte 0.
      if (meta.fmt == Fmt::time_bcd3r) {
        s.push_back(bcdPack(static_cast<uint8_t>(second)), false);
        s.push_back(bcdPack(static_cast<uint8_t>(minute)), false);
        s.push_back(bcdPack(static_cast<uint8_t>(hour)), false);
      } else {
        s.push_back(static_cast<uint8_t>(second), false);
        s.push_back(static_cast<uint8_t>(minute), false);
        s.push_back(static_cast<uint8_t>(hour), false);
      }
      return s;
    }
    case Fmt::time_bcd2r: {
      int hour, minute;
      if (!parseTimeString(*str, false, &hour, &minute)) return Sequence{};
      // REV layout: minute first, both bytes BCD.
      s.push_back(bcdPack(static_cast<uint8_t>(minute)), false);
      s.push_back(bcdPack(static_cast<uint8_t>(hour)), false);
      return s;
    }
    case Fmt::time_bin3: {
      int hour, minute, second;
      if (!parseTimeString(*str, true, &hour, &minute, &second)) {
        return Sequence{};
      }
      s.push_back(static_cast<uint8_t>(hour), false);
      s.push_back(static_cast<uint8_t>(minute), false);
      s.push_back(static_cast<uint8_t>(second), false);
      return s;
    }
    case Fmt::time_bin2: {
      int hour, minute;
      if (!parseTimeString(*str, false, &hour, &minute)) return Sequence{};
      s.push_back(static_cast<uint8_t>(hour), false);
      s.push_back(static_cast<uint8_t>(minute), false);
      return s;
    }
    case Fmt::time_bin2r: {
      int hour, minute;
      if (!parseTimeString(*str, false, &hour, &minute)) return Sequence{};
      s.push_back(static_cast<uint8_t>(minute), false);
      s.push_back(static_cast<uint8_t>(hour), false);
      return s;
    }
    case Fmt::time_min2: {
      int hour, minute;
      if (!parseTimeString(*str, false, &hour, &minute)) return Sequence{};
      const unsigned total = static_cast<unsigned>(hour) * 60 + minute;
      s.push_back(static_cast<uint8_t>(total & 0xff), false);
      s.push_back(static_cast<uint8_t>(total >> 8), false);
      return s;
    }
    case Fmt::time_trunc10:
    case Fmt::time_trunc30:
    case Fmt::time_trunc15: {
      int step = 10;
      if (meta.fmt == Fmt::time_trunc30) {
        step = 30;
      } else if (meta.fmt == Fmt::time_trunc15) {
        step = 15;
      }
      int hour, minute;
      if (!parseTimeString(*str, false, &hour, &minute)) return Sequence{};
      // Round to the nearest step (nearest 10/30/15-minute mark).
      const int steps = (hour * 60 + minute + step / 2) / step;
      if (steps > 24 * 60 / step) return Sequence{};
      s.push_back(static_cast<uint8_t>(steps), false);
      return s;
    }
    case Fmt::pin2: {
      int pin_value;
      if (!parsePinString(*str, &pin_value)) return Sequence{};
      s.push_back(bcdPack(static_cast<uint8_t>(pin_value / 100)), false);
      s.push_back(bcdPack(static_cast<uint8_t>(pin_value % 100)), false);
      return s;
    }
    case Fmt::tem_p: {
      // "-" null display token, then "GG-NNN" → slave-packed word.
      if (*str == "-") {
        pushReplacement(s, meta);
        return s;
      }
      const size_t dash = str->find('-');
      if (dash == std::string::npos) return Sequence{};
      int grp, num;
      if (!parseIntToken(*str, 0, dash, &grp) ||
          !parseIntToken(*str, dash + 1, str->size(), &num)) {
        return Sequence{};
      }
      if (grp < 0 || grp > 0x1f || num < 0 || num > 0x7f) return Sequence{};
      const unsigned value =
          (static_cast<unsigned>(grp) << 7) | static_cast<unsigned>(num);
      s.push_back(static_cast<uint8_t>(value & 0xff), false);
      s.push_back(static_cast<uint8_t>((value >> 8) & 0xff), false);
      return s;
    }
    case Fmt::none:
    default:
      break;
  }
  return s;
}

}  // namespace

/* --- Core Operations --- */

std::string renderEbusString(ByteView data) {
  std::string out;
  for (size_t i = 0; i < data.size(); ++i) {
    const uint8_t b = data[i];
    if (b == 0x00) break;  // NUL terminates, rest is padding
    if (b < 0x20)
      out.push_back(' ');
    else if (b >= 0x7f)
      out.push_back('?');
    else
      out.push_back(static_cast<char>(b));
  }
  return out;
}

std::optional<DataValue> decode(DataType dt, ByteView data, Endian e,
                                bool use_sentinel) {
  const Meta* m = metaFor(dt);
  if (!m || data.size() < m->size) return std::nullopt;

  if (m->fmt != Fmt::none) {
    // Byte order is baked into each format (REV layouts included); the
    // Endian parameter does not apply to string-semantic types.
    return decodeString(*m, ByteView(data.data(), m->size), use_sentinel);
  }

  const bool flip = m->reversed ? (e == Endian::little) : (e == Endian::big);

  if (!m->is_numeric) {
    return renderEbusString(ByteView(data.data(), m->size));
  }

  // Check for Replacement Values (Sentinels)
  uint32_t bit_pattern = 0;
  switch (m->size) {
    case 1:
      bit_pattern = data[0];
      break;
    case 2:
      bit_pattern = readInt<uint16_t>(data, flip);
      break;
    case 3:
      bit_pattern = readInt24(data, flip);
      break;
    case 4:
      bit_pattern = readInt<uint32_t>(data, flip);
      break;
    default:
      break;
  }

  if (!m->is_float && m->has_replacement && use_sentinel &&
      bit_pattern == m->replacement_value) {
    return nullValue();
  }

  if (dt == DataType::bcd8) {
    uint8_t val = data[0];
    if ((val & 0x0f) > 9 || ((val >> 4) & 0x0f) > 9) return std::nullopt;
    return static_cast<uint8_t>((val >> 4) * 10 + (val & 0x0f));
  }

  // Multi-byte BCD digit pairs, little-endian pairs (first byte = least
  // significant pair): any 0xff byte is the replacement/null marker;
  // nibbles above 9 are a protocol error.
  if (dt == DataType::bcd16 || dt == DataType::bcd24 || dt == DataType::bcd32) {
    const size_t len = m->size;
    if (use_sentinel) {
      for (size_t i = 0; i < len; ++i) {
        if (data[i] == detail::DataTypeLimits::null_sentinel)
          return nullValue();
      }
    }
    uint32_t value = 0;
    uint32_t exp = 1;
    for (size_t i = 0; i < len; ++i) {
      const uint8_t val = data[i];
      if ((val & 0x0f) > 9 || ((val >> 4) & 0x0f) > 9) return std::nullopt;
      value += static_cast<uint32_t>(((val >> 4) * 10 + (val & 0x0f))) * exp;
      exp *= 100;
    }
    if (len == 2) return static_cast<uint16_t>(value);
    return value;
  }

  // Read the raw integer value from bytes
  int64_t raw_val = 0;
  if (m->size == 1)
    raw_val = m->is_signed ? static_cast<int8_t>(data[0]) : data[0];
  else if (m->size == 2)
    raw_val = m->is_signed
                  ? static_cast<int16_t>(readInt<uint16_t>(data, flip))
                  : static_cast<int64_t>(readInt<uint16_t>(data, flip));
  else if (m->size == 3) {
    const uint32_t u24 = readInt24(data, flip);
    if (m->is_signed)
      raw_val = static_cast<int64_t>(
          (u24 & 0x800000u) ? static_cast<int32_t>(u24 | 0xff000000u)
                            : static_cast<int32_t>(u24));
    else
      raw_val = static_cast<int64_t>(u24);
  } else if (m->size == 4)
    raw_val = m->is_signed
                  ? static_cast<int32_t>(readInt<uint32_t>(data, flip))
                  : static_cast<int64_t>(readInt<uint32_t>(data, flip));

  if (m->is_float) {
    uint32_t raw = readInt<uint32_t>(data, flip);
    float f;
    std::memcpy(&f, &raw, detail::FixedPointLimits::float_size);
    return f;
  }

  // Handle scaled integer types (DATA1C, DATA2B, etc.)
  if (m->scale.num != 1 || m->scale.den != 1) {
    // Store as int64_t fixed-point: value * FIXED_POINT_SCALE
    return static_cast<int64_t>((static_cast<int64_t>(raw_val) * m->scale.num *
                                 detail::FixedPointLimits::fixed_point_scale) /
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

  if (m->fmt != Fmt::none) {
    // Replacement bytes are valid protocol data (a null result), like the
    // BCD 0xff rule below.
    DateTimeParts p;
    return parseWire(*m, ByteView(data.data(), m->size), true, &p) !=
           ParseState::invalid;
  }

  if (dt == DataType::bcd8) {
    uint8_t val = data[0];
    // Sentinel 0xFF is valid as a "null" result, not a protocol error.
    if (val == detail::DataTypeLimits::null_sentinel) return true;
    return (val & 0x0f) <= 9 && ((val >> 4) & 0x0f) <= 9;
  }
  if (dt == DataType::bcd16 || dt == DataType::bcd24 || dt == DataType::bcd32) {
    for (size_t i = 0; i < m->size; ++i) {
      const uint8_t val = data[i];
      if (val == detail::DataTypeLimits::null_sentinel) continue;
      if ((val & 0x0f) > 9 || ((val >> 4) & 0x0f) > 9) return false;
    }
    return true;
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

  if (m->fmt != Fmt::none) return encodeString(*m, value);

  const bool flip = m->reversed ? (e == Endian::little) : (e == Endian::big);

  // Handle numeric null sentinel encoding
  if (m->is_numeric && isNull(value)) {
    if (!m->has_replacement) return Sequence{};  // no sentinel defined
    if (m->size == 1) {
      s.push_back(static_cast<uint8_t>(m->replacement_value), false);
    } else if (m->size == 2) {
      writeInt<uint16_t>(s, static_cast<uint16_t>(m->replacement_value), flip);
    } else if (m->size == 3) {
      writeInt24(s, static_cast<uint32_t>(m->replacement_value), flip);
    } else if (m->size == 4) {
      writeInt<uint32_t>(s, static_cast<uint32_t>(m->replacement_value), flip);
    } else {
      // Generic: write low bytes of replacement_value in little-first order
      for (uint8_t i = 0; i < m->size; ++i)
        s.push_back(
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
      while (s.size() < m->size) s.push_back(0, false);
    }
    return s;
  }

  // Handle BCD: value -> digit pairs, least significant pair first
  if (dt == DataType::bcd8 || dt == DataType::bcd16 || dt == DataType::bcd24 ||
      dt == DataType::bcd32) {
    static constexpr int64_t kMax[] = {99, 9999, 999999, 99999999};
    const size_t slot = dt == DataType::bcd8    ? 0
                        : dt == DataType::bcd16 ? 1
                        : dt == DataType::bcd24 ? 2
                                                : 3;
    int64_t v = asInt64(value);
    if (v < 0 || v > kMax[slot]) return s;  // empty sequence = error
    uint32_t u = static_cast<uint32_t>(v);
    uint32_t exp = 1;
    for (size_t i = 0; i < m->size; ++i) {
      const uint8_t pair = static_cast<uint8_t>((u / exp) % 100);
      s.push_back(static_cast<uint8_t>(((pair / 10) << 4) | (pair % 10)),
                  false);
      exp *= 100;
    }
    return s;
  }

  // Handle Floats
  if (m->is_float) {
    float f = asFloat(value);
    uint32_t raw;
    std::memcpy(&raw, &f, detail::FixedPointLimits::float_size);
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
    raw_int = static_cast<int64_t>(
        std::round((static_cast<float>(std::get<int64_t>(value)) /
                    detail::FixedPointLimits::fixed_point_scale) *
                   m->scale.den / m->scale.num));
  } else {
    raw_int = integerScale(asInt64(value), m->scale.den, m->scale.num);
  }

  // Prevent bit-truncation by checking ranges
  if (!validateRange(raw_int, m->size, m->is_signed)) return s;

  if (m->size == 1)
    s.push_back(static_cast<uint8_t>(raw_int), false);
  else if (m->size == 2)
    writeInt<uint16_t>(s, static_cast<uint16_t>(raw_int), flip);
  else if (m->size == 3)
    writeInt24(s, static_cast<uint32_t>(raw_int), flip);
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
          return static_cast<float>(arg) /
                 detail::FixedPointLimits::fixed_point_scale;
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
          return (arg + detail::FixedPointLimits::fixed_point_scale / 2) /
                 detail::FixedPointLimits::fixed_point_scale;
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
  std::string& out;

  void operator()(std::monostate) const { out += "null"; }
  void operator()(const std::string& s) const { out += s; }

  void operator()(int64_t val) const {
    char buffer[64];
    const char* end = formatFloat(
        static_cast<float>(val) / detail::FixedPointLimits::fixed_point_scale,
        detail::FormattingLimits::default_precision, buffer, sizeof(buffer),
        detail::FormattingLimits::float_lower_threshold,
        detail::FormattingLimits::float_upper_threshold);
    out.append(buffer, static_cast<size_t>(end - buffer));
  }

  void operator()(float val) const {
    char buffer[64];
    const char* end = formatFloat(
        val, detail::FormattingLimits::default_precision, buffer,
        sizeof(buffer), detail::FormattingLimits::float_lower_threshold,
        detail::FormattingLimits::float_upper_threshold);
    out.append(buffer, static_cast<size_t>(end - buffer));
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  void operator()(T val) const {
    char buf[24];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec == std::errc{}) {
      out.append(buf, static_cast<size_t>(ptr - buf));
    }
  }
};

void toString(std::string& out, const DataValue& value, std::string_view unit) {
  size_t start_pos = out.size();
  std::visit(ToStringValueVisitor{out}, value);
  std::string_view appended = std::string_view(out).substr(start_pos);
  if (!unit.empty() && appended != "null" && !appended.empty()) {
    out += " ";
    out += unit;
  }
}

void toHexString(std::string& out, const DataValue& value, char separator) {
  if (const std::string* s = std::get_if<std::string>(&value)) {
    if (s->empty()) return;
    for (size_t i = 0; i < s->size(); ++i) {
      if (i > 0 && i % 2 == 0 && separator != 0) out += separator;
      out += (*s)[i];
    }
    return;
  }
  toString(out, value);
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
        if constexpr (std::is_floating_point_v<T>) return DataType::float32;
        if constexpr (std::is_same_v<T, std::string>) return DataType::char8;
        return DataType::error;
      },
      value);
}

std::vector<DataTypeInfo> getSupportedDataTypes() {
  std::vector<DataTypeInfo> types;
  types.reserve(sizeof(meta_table) / sizeof(meta_table[0]));
  std::for_each(std::begin(meta_table), std::end(meta_table),
                [&](const auto& m) {
                  DataTypeInfo info;
                  static_cast<DataTypeInfoBase&>(info) = m;
                  info.factor = static_cast<float>(m.scale.num) / m.scale.den;
                  types.push_back(info);
                });
  return types;
}

void fetchSupportedDataTypes(
    std::function<void(const DataTypeInfo&)> callback) {
  if (!callback) return;
  std::for_each(std::begin(meta_table), std::end(meta_table),
                [&](const auto& m) {
                  DataTypeInfo info;
                  static_cast<DataTypeInfoBase&>(info) = m;
                  info.factor = static_cast<float>(m.scale.num) / m.scale.den;
                  callback(info);
                });
}

const char* dataTypeToString(DataType data_type) noexcept {
  auto m = getMeta(data_type);
  return m ? m->name : "UNKNOWN";
}

DataType stringToDataType(const char* str) {
  auto it = std::lower_bound(
      std::begin(meta_table), std::end(meta_table), str,
      [](const Meta& m, const char* s) { return std::strcmp(m.name, s) < 0; });
  if (it != std::end(meta_table) && std::strcmp(it->name, str) == 0)
    return it->dt;
  return DataType::error;
}

void DataTypeInfo::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeField("type", static_cast<int32_t>(dt));
  writer.writeField("name", name);
  writer.writeField("size", size);
  writer.writeField("is_numeric", is_numeric);
  writer.writeField("is_signed", is_signed);
  writer.writeField("is_float", is_float);
  writer.writeField("has_replacement", has_replacement);
  writer.writeField("replacement_value", replacement_value);
  writer.writeFieldFloat("factor", factor,
                         detail::FormattingLimits::detailed_precision);
}

void fetchSupportedDataTypes(const JsonChunkVisitor& visitor, bool pretty) {
  detail::JsonWriter writer(visitor, pretty);
  auto scope = writer.arrayScope();
  fetchSupportedDataTypes(
      [&](const DataTypeInfo& info) { writer.writeValue(info); });
}

void decodeToJson(const JsonChunkVisitor& visitor, DataType dt, ByteView data,
                  bool pretty) {
  detail::JsonWriter writer(visitor, pretty);
  auto scope = writer.objectScope();

  auto meta_opt = getMeta(dt);
  if (!meta_opt) {
    writer.writeField("error", "Invalid DataType");
  } else {
    const auto& meta = *meta_opt;
    writer.writeField("type", static_cast<int32_t>(meta.dt));
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
}

}  // namespace ebus
