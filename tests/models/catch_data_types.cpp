/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cctype>
#include <cmath>
#include <ebus/data_types.hpp>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <limits>
#include <vector>

using namespace ebus;

TEST_CASE("Datatypes: BCD", "[models][datatypes]") {
  for (uint8_t v = 0; v <= 99; ++v) {
    Sequence bytes = ebus::encode(ebus::DataType::bcd, v);
    auto decoded = ebus::decode(ebus::DataType::bcd, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == v);
  }
  std::vector<uint8_t> invalid = {0xaa};
  REQUIRE_FALSE(ebus::decode(ebus::DataType::bcd, invalid).has_value());
}

TEST_CASE("Datatypes: uint8/int8/data1b/data1c", "[models][datatypes]") {
  for (uint16_t v = 0; v <= 0xff; ++v) {
    uint8_t value = static_cast<uint8_t>(v);
    Sequence bytes = ebus::encode(DataType::uint8, value);
    auto decoded = ebus::decode(DataType::uint8, bytes);
    REQUIRE(decoded.has_value());
    if (v == 0xff) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
    REQUIRE(bytes.size() == 1);
    REQUIRE(bytes[0] == value);
  }

  for (int16_t v = -128; v <= 127; ++v) {
    int8_t value = static_cast<int8_t>(v);
    Sequence bytes = ebus::encode(DataType::int8, value);
    auto decoded = ebus::decode(DataType::int8, bytes);
    REQUIRE(decoded.has_value());
    if (v == -128) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (int16_t v = -128; v <= 127; v += 17) {
    float value = static_cast<float>(v);
    Sequence bytes = ebus::encode(DataType::data1b, value);
    auto decoded = ebus::decode(DataType::data1b, bytes);
    REQUIRE(decoded.has_value());
    if (v == -128) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5f);
    }
  }

  for (uint16_t v = 0; v <= 255; v += 17) {
    float value = static_cast<float>(v) / 2.0;
    Sequence bytes = ebus::encode(DataType::data1c, value);
    auto decoded = ebus::decode(DataType::data1c, bytes);
    REQUIRE(decoded.has_value());
    if (v == 255) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5);
    }
  }
}

TEST_CASE("DATA1C resolution", "[models][datatypes]") {
  // DATA1C: 0.5 resolution
  // 0x01 -> 0.5
  auto d1 = ebus::decode(DataType::data1c, ebus::ByteView({0x01}));
  REQUIRE(d1);
  REQUIRE(std::fabs(ebus::asFloat(*d1) - 0.5) < 1e-5);

  // 0x02 -> 1.0
  auto d2 = ebus::decode(DataType::data1c, ebus::ByteView({0x02}));
  REQUIRE(d2);
  REQUIRE(std::fabs(ebus::asFloat(*d2) - 1.0) < 1e-5);
}

TEST_CASE("Datatypes: 16-bit and data2", "[models][datatypes]") {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {
    uint16_t value = static_cast<uint16_t>(v);
    Sequence bytes = ebus::encode(DataType::uint16, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint16, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == 0xffff) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (uint32_t v = 0; v <= 0xffff; v += 257) {
    uint16_t value = static_cast<uint16_t>(v);
    Sequence bytes = ebus::encode(DataType::uint16r, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint16r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == 0xffff) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    Sequence bytes = ebus::encode(DataType::int16, value, Endian::little);
    auto decoded = ebus::decode(DataType::int16, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    Sequence bytes = ebus::encode(DataType::int16r, value, Endian::little);
    auto decoded = ebus::decode(DataType::int16r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    float value = static_cast<float>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2b, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2b, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5f);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    float value = static_cast<float>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2br, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2br, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5f);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    float value = static_cast<float>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2c, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2c, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-3);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    float value = static_cast<float>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2cr, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2cr, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5f);
    }
  }
}

TEST_CASE("Datatypes: 32-bit ints and floats", "[models][datatypes]") {
  std::vector<uint32_t> u32_vals = {0, 1, 0xffffffff, 0x12345678, 0x80000000};
  for (uint32_t value : u32_vals) {
    Sequence bytes = ebus::encode(DataType::uint32, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint32, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (value == 0xffffffff) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == static_cast<int64_t>(value));
    }
  }

  for (uint32_t value : u32_vals) {
    Sequence bytes = ebus::encode(DataType::uint32r, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint32r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (value == 0xffffffff) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == static_cast<int64_t>(value));
    }
  }

  std::vector<int32_t> i32_vals = {0,
                                   1,
                                   -1,
                                   std::numeric_limits<int32_t>::max(),
                                   std::numeric_limits<int32_t>::min(),
                                   0x12345678};
  for (int32_t value : i32_vals) {
    Sequence bytes = ebus::encode(DataType::int32, value, Endian::little);
    auto decoded = ebus::decode(DataType::int32, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (value == std::numeric_limits<int32_t>::min()) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  for (int32_t value : i32_vals) {
    Sequence bytes = ebus::encode(DataType::int32r, value, Endian::little);
    auto decoded = ebus::decode(DataType::int32r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (value == std::numeric_limits<int32_t>::min()) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(ebus::asInt64(*decoded) == value);
    }
  }

  std::vector<float> float_vals = {0.0f,
                                   1.0f,
                                   -1.0f,
                                   123.456f,
                                   -789.123f,
                                   std::numeric_limits<float>::infinity(),
                                   -std::numeric_limits<float>::infinity(),
                                   std::nanf("")};
  for (float value : float_vals) {
    Sequence bytes = ebus::encode(DataType::float4, value, Endian::little);
    auto decoded = ebus::decode(DataType::float4, bytes, Endian::little);
    if (std::isnan(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isnan(ebus::asFloat(*decoded)));
    } else if (std::isinf(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isinf(ebus::asFloat(*decoded)));
      REQUIRE(std::signbit(value) == std::signbit(ebus::asFloat(*decoded)));
    } else {
      REQUIRE(decoded.has_value());
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5);
    }
  }

  for (float value : float_vals) {
    Sequence bytes = ebus::encode(DataType::float4r, value, Endian::little);
    auto decoded = ebus::decode(DataType::float4r, bytes, Endian::little);
    if (std::isnan(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isnan(ebus::asFloat(*decoded)));
    } else if (std::isinf(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isinf(ebus::asFloat(*decoded)));
      REQUIRE(std::signbit(value) == std::signbit(ebus::asFloat(*decoded)));
    } else {
      REQUIRE(decoded.has_value());
      REQUIRE(std::fabs(ebus::asFloat(*decoded) - value) < 1e-5);
    }
  }
}

TEST_CASE("Datatypes: char and hex", "[models][datatypes]") {
  std::vector<std::string> test_strings = {"", "A", "Hello", "ebus123",
                                           std::string(8, 'Z')};
  for (const std::string& str : test_strings) {
    Sequence bytes = ebus::encode(DataType::char8, str);
    auto decoded = ebus::decode(DataType::char8, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(std::get<std::string>(*decoded).find(str) == 0);
  }

  std::vector<std::string> hex_strings = {"",       "00",     "FF",      "1234",
                                          "abcdef", "ABCDEF", "deadbeef"};
  for (const std::string& str : hex_strings) {
    std::string str_lower = str;
    std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(),
                   ::tolower);

    Sequence bytes = ebus::encode(DataType::hex8, str_lower);
    auto decoded = ebus::decode(DataType::hex8, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(std::get<std::string>(*decoded).find(str_lower) == 0);
  }
}

TEST_CASE("Datatypes: metadata", "[models][datatypes]") {
  auto types = ebus::getSupportedDataTypes();
  REQUIRE_FALSE(types.empty());
  for (auto dt : types) {
    auto meta = ebus::getMeta(dt.dt);
    REQUIRE(meta.has_value());
    REQUIRE(meta->dt == dt.dt);
    REQUIRE(std::string(meta->name) != "UNKNOWN");
    REQUIRE(meta->size == ebus::sizeOfDataType(dt.dt));
  }
}

TEST_CASE("Datatypes: auto detection", "[models][datatypes]") {
  // uint8
  DataValue v1 = static_cast<uint8_t>(42);
  Sequence s1 = ebus::encode(DataType::auto_detect, v1);
  REQUIRE(s1.size() == 1);
  REQUIRE(s1[0] == 42);

  // float4 (default for float)
  DataValue v2 = 21.5f;
  Sequence s2 = ebus::encode(DataType::auto_detect, v2);
  REQUIRE(s2.size() == 4);
  auto d2 = ebus::decode(DataType::float4, s2);
  REQUIRE(d2);
  REQUIRE(std::fabs(ebus::asFloat(*d2) - 21.5f) < 1e-5f);

  // char8 (default for string)
  DataValue v3 = std::string("EBUS");
  Sequence s3 = ebus::encode(DataType::auto_detect, v3);
  REQUIRE(s3.size() == 8);
  REQUIRE(std::string(reinterpret_cast<const char*>(s3.data()), 4) == "EBUS");

  // Fixed-point int64_t (should not auto-detect to a specific eBUS DataType)
  DataValue v4 = 123456789LL;  // Example fixed-point value
  Sequence s4 = ebus::encode(DataType::auto_detect, v4);
  REQUIRE(s4.empty());  // Should return empty sequence as auto-detect fails
}

TEST_CASE("Datatypes: getAs", "[models][datatypes]") {
  // Valid conversion
  DataValue v1 = static_cast<int32_t>(200);
  auto r1 = ebus::getAs<uint8_t>(v1);
  REQUIRE(r1.has_value());
  REQUIRE(*r1 == 200);

  // Overflow check
  DataValue v2 = static_cast<int32_t>(300);
  auto r2 = ebus::getAs<uint8_t>(v2);
  REQUIRE(!r2.has_value());

  // Underflow check
  DataValue v3 = static_cast<int32_t>(-1);
  auto r3 = ebus::getAs<uint8_t>(v3);
  REQUIRE(!r3.has_value());
}

TEST_CASE("Datatypes: encode range checking", "[models][datatypes]") {
  // uint8 range is 0-255. 256 is out of range.
  REQUIRE(ebus::encode(DataType::uint8, 255).size() == 1);
  REQUIRE(ebus::encode(DataType::uint8, 256).empty());
  REQUIRE(ebus::encode(DataType::uint8, -1).empty());

  // int8 range is -128 to 127.
  REQUIRE(ebus::encode(DataType::int8, -128).size() == 1);
  REQUIRE(ebus::encode(DataType::int8, 127).size() == 1);
  REQUIRE(ebus::encode(DataType::int8, 128).empty());

  // uint16 range is 0-65535.
  REQUIRE(ebus::encode(DataType::uint16, 65536).empty());
}

TEST_CASE("Datatypes: Fixed-point int64_t conversions", "[models][datatypes]") {
  // Test DATA2B (resolution 1/256)
  // Value 12.5 -> raw 3200 (0x0C80)
  // Fixed-point: 12.5 * 1,000,000 = 12,500,000
  static const std::array<uint8_t, 2> data2b_12_5 = {0x80, 0x0C};
  DataValue fixed_data2b =
      ebus::decode(DataType::data2b,
                   ebus::ByteView(data2b_12_5.data(), data2b_12_5.size()))
          .value();
  REQUIRE(std::holds_alternative<int64_t>(fixed_data2b));
  REQUIRE(ebus::asFloat(fixed_data2b) == Catch::Approx(12.5f));
  // The original code had a comment: "Should round to nearest integer".
  // 12.5 rounds to 13.
  REQUIRE(ebus::asInt64(fixed_data2b) ==
          13);  // Should round to nearest integer

  // Test DATA1C (resolution 0.5)
  // Value 1.5 -> raw 3 (0x03)
  // Fixed-point: 1.5 * 1,000,000 = 1,500,000
  DataValue fixed_data1c =
      ebus::decode(DataType::data1c,
                   ebus::ByteView(std::array<uint8_t, 1>{0x03}.data(), 1))
          .value();
  REQUIRE(std::holds_alternative<int64_t>(fixed_data1c));
  REQUIRE(ebus::asFloat(fixed_data1c) == Catch::Approx(1.5f));
  // 1.5 rounds to 2.
  REQUIRE(ebus::asInt64(fixed_data1c) == 2);  // Should round to nearest integer

  // Test negative fixed-point
  // DATA2B: -12.5 -> raw -3200 (0xF380)
  // Fixed-point: -12.5 * 1,000,000 = -12,500,000
  static const std::array<uint8_t, 2> data2b_neg_12_5 = {0x80, 0xF3};
  DataValue fixed_neg_data2b =
      ebus::decode(DataType::data2b, ebus::ByteView(data2b_neg_12_5.data(),
                                                    data2b_neg_12_5.size()))
          .value();
  REQUIRE(std::holds_alternative<int64_t>(fixed_neg_data2b));
  REQUIRE(ebus::asFloat(fixed_neg_data2b) == Catch::Approx(-12.5f));
  // -12.5 rounds to -12.
  REQUIRE(ebus::asInt64(fixed_neg_data2b) ==
          -12);  // Should round to nearest integer

  // Test zero fixed-point.
  static const std::array<uint8_t, 2> data2b_zero = {0x00, 0x00};
  DataValue fixed_zero =
      ebus::decode(DataType::data2b,
                   ebus::ByteView(data2b_zero.data(), data2b_zero.size()))
          .value();
  REQUIRE(std::holds_alternative<int64_t>(fixed_zero));
  REQUIRE(ebus::asFloat(fixed_zero) == Catch::Approx(0.0f));
  REQUIRE(ebus::asInt64(fixed_zero) == 0);

  // Test encoding fixed-point back to bytes
  // Encode 12.5 (as fixed-point int64_t) to DATA2B
  Sequence encoded_fixed = ebus::encode(
      DataType::data2b, static_cast<int64_t>(12.5f * FIXED_POINT_SCALE));
  REQUIRE(encoded_fixed.size() == 2);
  REQUIRE(encoded_fixed[0] == 0x80);
  REQUIRE(encoded_fixed[1] == 0x0C);

  // Encode 1.5 (as fixed-point int64_t) to DATA1C
  Sequence encoded_fixed_1c = ebus::encode(
      DataType::data1c, static_cast<int64_t>(1.5f * FIXED_POINT_SCALE));
  REQUIRE(encoded_fixed_1c.size() == 1);
  REQUIRE(encoded_fixed_1c[0] == 0x03);

  // Encoding a float that would be represented as fixed-point
  Sequence encoded_float_to_fixed = ebus::encode(DataType::data2b, 12.5f);
  REQUIRE(encoded_float_to_fixed.size() == 2);
  REQUIRE(encoded_float_to_fixed[0] == 0x80);
  REQUIRE(encoded_float_to_fixed[1] == 0x0C);

  // Encoding an integer that would be represented as fixed-point
  Sequence encoded_int_to_fixed = ebus::encode(DataType::data2b, 12);
  REQUIRE(encoded_int_to_fixed.size() == 2);
  REQUIRE(encoded_int_to_fixed[0] == 0x00);  // 12 * 256 = 3072 = 0x0C00
  REQUIRE(encoded_int_to_fixed[1] == 0x0C);
}

TEST_CASE("Datatypes: isValid", "[models][datatypes]") {
  // BCD validation
  static const std::array<uint8_t, 1> bcd_12 = {0x12};
  REQUIRE(ebus::isValid(DataType::bcd,
                        ebus::ByteView(bcd_12.data(), bcd_12.size())));  // "12"
  static const std::array<uint8_t, 1> bcd_1a = {0x1a};
  REQUIRE(!ebus::isValid(
      DataType::bcd,
      ebus::ByteView(bcd_1a.data(), bcd_1a.size())));  // Invalid nibble
  static const std::array<uint8_t, 1> bcd_ff = {0xff};
  REQUIRE(ebus::isValid(
      DataType::bcd,
      ebus::ByteView(
          bcd_ff.data(),
          bcd_ff.size())));  // Spec Replacement Value is valid protocol data

  // Size validation
  static const std::array<uint8_t, 1> uint16_too_short = {0x01};
  REQUIRE(!ebus::isValid(
      DataType::uint16, ebus::ByteView(uint16_too_short.data(),
                                       uint16_too_short.size())));  // Too short
}

TEST_CASE("Datatypes: sentinels", "[models][datatypes]") {
  // BCD 0xFF -> null
  static const std::array<uint8_t, 1> bcd_ff_sentinel = {0xff};
  auto v1 = ebus::decode(DataType::bcd, ebus::ByteView(bcd_ff_sentinel.data(),
                                                       bcd_ff_sentinel.size()));
  REQUIRE(v1);
  REQUIRE(ebus::isNull(*v1));

  // DATA1B 0x80 -> null
  auto v2 = ebus::decode(DataType::data1b, ebus::ByteView({0x80}));
  REQUIRE(v2);
  REQUIRE(ebus::isNull(*v2));

  // DATA1C 0xFF -> null
  static const std::array<uint8_t, 1> data1c_ff_sentinel = {0xff};
  auto v3 = ebus::decode(
      DataType::data1c,
      ebus::ByteView(data1c_ff_sentinel.data(), data1c_ff_sentinel.size()));
  REQUIRE(v3);
  REQUIRE(ebus::isNull(*v3));

  // UINT16 0xFFFF -> null
  static const std::array<uint8_t, 2> uint16_ff_sentinel = {0xff, 0xff};
  auto v4 = ebus::decode(
      DataType::uint16,
      ebus::ByteView(uint16_ff_sentinel.data(), uint16_ff_sentinel.size()));
  REQUIRE(v4);
  REQUIRE(ebus::isNull(*v4));
}

TEST_CASE("Datatypes: isNumeric(DataType)", "[models][datatypes]") {
  REQUIRE(ebus::isNumeric(DataType::uint8) == true);
  REQUIRE(ebus::isNumeric(DataType::int8) == true);
  REQUIRE(ebus::isNumeric(DataType::float4) == true);
  REQUIRE(ebus::isNumeric(DataType::data1b) == true);
  REQUIRE(ebus::isNumeric(DataType::bcd) == true);
  REQUIRE(ebus::isNumeric(DataType::uint16) == true);
  REQUIRE(ebus::isNumeric(DataType::int16) == true);
  REQUIRE(ebus::isNumeric(DataType::uint32) == true);
  REQUIRE(ebus::isNumeric(DataType::int32) == true);
  REQUIRE(ebus::isNumeric(DataType::data1c) == true);
  REQUIRE(ebus::isNumeric(DataType::data2b) == true);

  REQUIRE(ebus::isNumeric(DataType::char1) == false);
  REQUIRE(ebus::isNumeric(DataType::char8) == false);
  REQUIRE(ebus::isNumeric(DataType::hex1) == false);
  REQUIRE(ebus::isNumeric(DataType::hex8) == false);
  REQUIRE(ebus::isNumeric(DataType::error) == false);
  REQUIRE(ebus::isNumeric(DataType::auto_detect) == false);
}

TEST_CASE("Datatypes: null sentinel encoding", "[models][datatypes]") {
  // BCD has replacement_value 0xff
  Sequence bcd_null = ebus::encode(DataType::bcd, ebus::nullValue());
  REQUIRE(bcd_null.size() == 1);
  REQUIRE(bcd_null[0] == 0xff);

  // uint8 has replacement_value 0xff
  Sequence uint8_null = ebus::encode(DataType::uint8, ebus::nullValue());
  REQUIRE(uint8_null.size() == 1);
  REQUIRE(uint8_null[0] == 0xff);

  // int8 has replacement_value 0x80
  Sequence int8_null = ebus::encode(DataType::int8, ebus::nullValue());
  REQUIRE(int8_null.size() == 1);
  REQUIRE(int8_null[0] == 0x80);

  // data1b has replacement_value 0x80
  Sequence data1b_null = ebus::encode(DataType::data1b, ebus::nullValue());
  REQUIRE(data1b_null.size() == 1);
  REQUIRE(data1b_null[0] == 0x80);

  // data1c has replacement_value 0xff
  Sequence data1c_null = ebus::encode(DataType::data1c, ebus::nullValue());
  REQUIRE(data1c_null.size() == 1);
  REQUIRE(data1c_null[0] == 0xff);

  // uint16 has replacement_value 0xffff
  Sequence uint16_null = ebus::encode(DataType::uint16, ebus::nullValue());
  REQUIRE(uint16_null.size() == 2);
  REQUIRE(uint16_null[0] == 0xff);
  REQUIRE(uint16_null[1] == 0xff);

  // int16 has replacement_value 0x8000 (little-endian: 0x00 0x80)
  Sequence int16_null = ebus::encode(DataType::int16, ebus::nullValue());
  REQUIRE(int16_null.size() == 2);
  REQUIRE(int16_null[0] == 0x00);
  REQUIRE(int16_null[1] == 0x80);

  // uint32 has replacement_value 0xffffffff
  Sequence uint32_null = ebus::encode(DataType::uint32, ebus::nullValue());
  REQUIRE(uint32_null.size() == 4);
  REQUIRE(uint32_null[0] == 0xff);
  REQUIRE(uint32_null[1] == 0xff);
  REQUIRE(uint32_null[2] == 0xff);
  REQUIRE(uint32_null[3] == 0xff);

  // int32 has replacement_value 0x80000000 (little-endian: 0x00 0x00 0x00 0x80)
  Sequence int32_null = ebus::encode(DataType::int32, ebus::nullValue());
  REQUIRE(int32_null.size() == 4);
  REQUIRE(int32_null[0] == 0x00);
  REQUIRE(int32_null[1] == 0x00);
  REQUIRE(int32_null[2] == 0x00);
  REQUIRE(int32_null[3] == 0x80);

  // float4 does not have replacement_value, so encoding null should return
  // empty sequence
  Sequence float4_null = ebus::encode(DataType::float4, ebus::nullValue());
  REQUIRE(float4_null.empty());

  // char1 does not have replacement_value, so encoding null should return empty
  // sequence
  Sequence char1_null = ebus::encode(DataType::char1, ebus::nullValue());
  REQUIRE(char1_null.empty());
}

TEST_CASE("Datatypes: all types roundtrip", "[models][datatypes]") {
  auto types = ebus::getSupportedDataTypes();
  for (auto dt : types) {
    auto meta = ebus::getMeta(dt.dt);
    REQUIRE(meta.has_value());

    // 1. Metadata check
    REQUIRE(meta->size == ebus::sizeOfDataType(dt.dt));

    // 2. Validity check with zero-buffer
    std::vector<uint8_t> buffer(meta->size, 0);
    REQUIRE(ebus::isValid(dt.dt, buffer));

    // 3. Decode -> Encode roundtrip
    auto decoded = ebus::decode(dt.dt, buffer);
    REQUIRE(decoded.has_value());

    Sequence encoded = ebus::encode(dt.dt, *decoded);
    REQUIRE(encoded.size() == meta->size);

    // 4. Data integrity check
    // For standard types with 1.0 factor and no float precision loss,
    // the value must be identical.
    if (!meta->is_float && std::abs(meta->factor - 1.0) < 1e-9) {
      auto re_decoded = ebus::decode(dt.dt, encoded);
      REQUIRE(re_decoded.has_value());
      // variant operator== checks type and value
      REQUIRE(*re_decoded == *decoded);
    }
  }
}