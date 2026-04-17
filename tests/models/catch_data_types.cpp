/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cctype>
#include <cmath>
#include <ebus/data_types.hpp>
#include <ebus/definitions.hpp>
#include <ebus/sequence.hpp>
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
    double_t value = static_cast<double_t>(v);
    Sequence bytes = ebus::encode(DataType::data1b, value);
    auto decoded = ebus::decode(DataType::data1b, bytes);
    REQUIRE(decoded.has_value());
    if (v == -128) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }

  for (uint16_t v = 0; v <= 255; v += 17) {
    double_t value = static_cast<double_t>(v) / 2.0;
    Sequence bytes = ebus::encode(DataType::data1c, value);
    auto decoded = ebus::decode(DataType::data1c, bytes);
    REQUIRE(decoded.has_value());
    if (v == 255) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }
}

TEST_CASE("DATA1C resolution", "[models][datatypes]") {
  // DATA1C: 0.5 resolution
  // 0x01 -> 0.5
  auto d1 = ebus::decode(DataType::data1c, ebus::ByteView({0x01}));
  REQUIRE(d1);
  REQUIRE(std::fabs(ebus::asDouble(*d1) - 0.5) < 1e-5);

  // 0x02 -> 1.0
  auto d2 = ebus::decode(DataType::data1c, ebus::ByteView({0x02}));
  REQUIRE(d2);
  REQUIRE(std::fabs(ebus::asDouble(*d2) - 1.0) < 1e-5);
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
    double_t value = static_cast<double_t>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2b, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2b, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2br, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2br, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2c, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2c, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2cr, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2cr, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    if (v == -32768) {
      REQUIRE(ebus::isNull(*decoded));
    } else {
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
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
    Sequence bytes = ebus::encode(DataType::float4,
                                  static_cast<double_t>(value), Endian::little);
    auto decoded = ebus::decode(DataType::float4, bytes, Endian::little);
    if (std::isnan(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isnan(ebus::asDouble(*decoded)));
    } else if (std::isinf(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isinf(ebus::asDouble(*decoded)));
      REQUIRE(std::signbit(value) == std::signbit(ebus::asDouble(*decoded)));
    } else {
      REQUIRE(decoded.has_value());
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }

  for (float value : float_vals) {
    Sequence bytes = ebus::encode(DataType::float4r,
                                  static_cast<double_t>(value), Endian::little);
    auto decoded = ebus::decode(DataType::float4r, bytes, Endian::little);
    if (std::isnan(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isnan(ebus::asDouble(*decoded)));
    } else if (std::isinf(value)) {
      REQUIRE(decoded.has_value());
      REQUIRE(std::isinf(ebus::asDouble(*decoded)));
      REQUIRE(std::signbit(value) == std::signbit(ebus::asDouble(*decoded)));
    } else {
      REQUIRE(decoded.has_value());
      REQUIRE(std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
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
    auto meta = ebus::getMeta(dt);
    REQUIRE(meta.has_value());
    REQUIRE(meta->dt == dt);
    REQUIRE(std::string(meta->name) != "UNKNOWN");
    REQUIRE(meta->size == ebus::sizeOfDataType(dt));
  }
}

TEST_CASE("Datatypes: auto detection", "[models][datatypes]") {
  // uint8
  DataValue v1 = static_cast<uint8_t>(42);
  Sequence s1 = ebus::encode(DataType::auto_detect, v1);
  REQUIRE(s1.size() == 1);
  REQUIRE(s1[0] == 42);

  // float4 (default for double)
  DataValue v2 = 21.5;
  Sequence s2 = ebus::encode(DataType::auto_detect, v2);
  REQUIRE(s2.size() == 4);
  auto d2 = ebus::decode(DataType::float4, s2);
  REQUIRE(d2);
  REQUIRE(std::fabs(ebus::asDouble(*d2) - 21.5) < 1e-5);

  // char8 (default for string)
  DataValue v3 = std::string("EBUS");
  Sequence s3 = ebus::encode(DataType::auto_detect, v3);
  REQUIRE(s3.size() == 8);
  REQUIRE(std::string(reinterpret_cast<const char*>(s3.data()), 4) == "EBUS");
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

TEST_CASE("Datatypes: isValid", "[models][datatypes]") {
  // BCD validation
  REQUIRE(ebus::isValid(DataType::bcd, ebus::ByteView({0x12})));  // "12"
  REQUIRE(
      !ebus::isValid(DataType::bcd, ebus::ByteView({0x1A})));  // Invalid nibble
  REQUIRE(ebus::isValid(
      DataType::bcd,
      ebus::ByteView(
          {0xFF})));  // Spec Replacement Value is valid protocol data

  // Size validation
  REQUIRE(
      !ebus::isValid(DataType::uint16, ebus::ByteView({0x01})));  // Too short
}

TEST_CASE("Datatypes: sentinels", "[models][datatypes]") {
  // BCD 0xFF -> null
  auto v1 = ebus::decode(DataType::bcd, ebus::ByteView({0xff}));
  REQUIRE(v1);
  REQUIRE(ebus::isNull(*v1));

  // DATA1B 0x80 -> null
  auto v2 = ebus::decode(DataType::data1b, ebus::ByteView({0x80}));
  REQUIRE(v2);
  REQUIRE(ebus::isNull(*v2));

  // DATA1C 0xFF -> null
  auto v3 = ebus::decode(DataType::data1c, ebus::ByteView({0xff}));
  REQUIRE(v3);
  REQUIRE(ebus::isNull(*v3));

  // UINT16 0xFFFF -> null
  auto v4 = ebus::decode(DataType::uint16, ebus::ByteView({0xff, 0xff}));
  REQUIRE(v4);
  REQUIRE(ebus::isNull(*v4));
}

TEST_CASE("Datatypes: all types roundtrip", "[models][datatypes]") {
  auto types = ebus::getSupportedDataTypes();
  for (auto dt : types) {
    auto meta = ebus::getMeta(dt);
    REQUIRE(meta.has_value());

    // 1. Metadata check
    REQUIRE(meta->size == ebus::sizeOfDataType(dt));

    // 2. Validity check with zero-buffer
    std::vector<uint8_t> buffer(meta->size, 0);
    REQUIRE(ebus::isValid(dt, buffer));

    // 3. Decode -> Encode roundtrip
    auto decoded = ebus::decode(dt, buffer);
    REQUIRE(decoded.has_value());

    Sequence encoded = ebus::encode(dt, *decoded);
    REQUIRE(encoded.size() == meta->size);

    // 4. Data integrity check
    // For standard types with 1.0 factor and no float precision loss,
    // the value must be identical.
    if (!meta->is_float && std::abs(meta->factor - 1.0) < 1e-9) {
      auto re_decoded = ebus::decode(dt, encoded);
      REQUIRE(re_decoded.has_value());
      // variant operator== checks type and value
      REQUIRE(*re_decoded == *decoded);
    }
  }
}