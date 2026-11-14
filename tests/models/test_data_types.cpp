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
    Sequence bytes = ebus::encode(ebus::DataType::bcd8, v);
    auto decoded = ebus::decode(ebus::DataType::bcd8, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == v);
  }
  std::vector<uint8_t> invalid = {0xaa};
  REQUIRE_FALSE(ebus::decode(ebus::DataType::bcd8, invalid).has_value());
}

TEST_CASE("Datatypes: BCD16/BCD24/BCD32", "[models][datatypes]") {
  // BCD16: LE digit pairs; "1234" -> wire 34 12.
  {
    Sequence bytes = ebus::encode(DataType::bcd16, static_cast<uint16_t>(1234));
    REQUIRE(bytes.size() == 2);
    REQUIRE(bytes[0] == 0x34);
    REQUIRE(bytes[1] == 0x12);
    auto decoded = ebus::decode(DataType::bcd16, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == 1234);

    std::vector<uint8_t> bad_nibble = {0x1a, 0x12};
    REQUIRE_FALSE(ebus::decode(DataType::bcd16, bad_nibble).has_value());

    // Encode range: > 9999 rejects with an empty sequence.
    REQUIRE(
        ebus::encode(DataType::bcd16, static_cast<uint16_t>(10000)).empty());
    REQUIRE(ebus::encode(DataType::bcd16, static_cast<uint16_t>(9999)).size() ==
            2);
  }

  // BCD24: "999999" -> wire 99 99 99.
  {
    Sequence bytes =
        ebus::encode(DataType::bcd24, static_cast<uint32_t>(999999));
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == 0x99);
    REQUIRE(bytes[1] == 0x99);
    REQUIRE(bytes[2] == 0x99);
    auto decoded = ebus::decode(DataType::bcd24, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == 999999);
    REQUIRE(
        ebus::encode(DataType::bcd24, static_cast<uint32_t>(1000000)).empty());
  }

  // BCD32: "12345678" -> wire 78 56 34 12.
  {
    Sequence bytes =
        ebus::encode(DataType::bcd32, static_cast<uint32_t>(12345678));
    REQUIRE(bytes.size() == 4);
    REQUIRE(bytes[0] == 0x78);
    REQUIRE(bytes[1] == 0x56);
    REQUIRE(bytes[2] == 0x34);
    REQUIRE(bytes[3] == 0x12);
    auto decoded = ebus::decode(DataType::bcd32, bytes);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == 12345678);
    REQUIRE(ebus::encode(DataType::bcd32, static_cast<uint32_t>(100000000))
                .empty());
  }

  // Any 0xff byte is the replacement marker -> whole-field null
  // (BCD wire format), regardless of position.
  REQUIRE(ebus::isNull(*ebus::decode(DataType::bcd16, ByteView({0xff, 0x12}))));
  REQUIRE(ebus::isNull(*ebus::decode(DataType::bcd16, ByteView({0x12, 0xff}))));
  REQUIRE(ebus::isNull(
      *ebus::decode(DataType::bcd24, ByteView({0x12, 0xff, 0x34}))));
  REQUIRE(ebus::isNull(
      *ebus::decode(DataType::bcd32, ByteView({0xff, 0xff, 0xff, 0xff}))));

  // Null encode: replacement bytes, least significant pair first.
  Sequence n16 = ebus::encode(DataType::bcd16, ebus::nullValue());
  REQUIRE(n16.size() == 2);
  REQUIRE(n16[0] == 0xff);
  REQUIRE(n16[1] == 0xff);

  // isValid: digit ranges and replacement bytes are valid protocol data.
  REQUIRE(ebus::isValid(DataType::bcd16, ByteView({0x34, 0x12})));
  REQUIRE_FALSE(ebus::isValid(DataType::bcd16, ByteView({0x3a, 0x12})));
  REQUIRE(ebus::isValid(DataType::bcd16, ByteView({0xff, 0x12})));
  REQUIRE(ebus::isValid(DataType::bcd32, ByteView({0xff, 0xff, 0xff, 0xff})));

  // Names and sizes.
  REQUIRE(ebus::stringToDataType("BCD8") == DataType::bcd8);
  REQUIRE(ebus::stringToDataType("BCD16") == DataType::bcd16);
  REQUIRE(ebus::stringToDataType("BCD24") == DataType::bcd24);
  REQUIRE(ebus::stringToDataType("BCD32") == DataType::bcd32);
  REQUIRE(std::string(ebus::dataTypeToString(DataType::bcd16)) == "BCD16");
  REQUIRE(ebus::sizeOfDataType(DataType::bcd8) == 1);
  REQUIRE(ebus::sizeOfDataType(DataType::bcd16) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::bcd24) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::bcd32) == 4);
  REQUIRE(ebus::isNumeric(DataType::bcd16) == true);
  REQUIRE(ebus::isNumeric(DataType::bcd32) == true);
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

TEST_CASE("Datatypes: 24-bit ints", "[models][datatypes]") {
  // ehp comp_starts wire evidence: 9e 01 00 decodes as 414 LE (not BCD)
  {
    std::vector<uint8_t> raw = {0x9e, 0x01, 0x00};
    auto decoded = ebus::decode(DataType::uint24, raw);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == 414);
  }

  // uint24 little-endian roundtrip + byte order
  for (uint32_t v = 0; v < 0xffffff; v += 0x40000) {
    Sequence bytes = ebus::encode(DataType::uint24, static_cast<uint32_t>(v),
                                  Endian::little);
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == static_cast<uint8_t>(v & 0xff));
    REQUIRE(bytes[1] == static_cast<uint8_t>((v >> 8) & 0xff));
    REQUIRE(bytes[2] == static_cast<uint8_t>((v >> 16) & 0xff));
    auto decoded = ebus::decode(DataType::uint24, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == static_cast<int64_t>(v));
  }

  // uint24 null sentinel (0xffffff)
  {
    std::vector<uint8_t> raw = {0xff, 0xff, 0xff};
    auto decoded = ebus::decode(DataType::uint24, raw);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::isNull(*decoded));
    Sequence encoded = ebus::encode(DataType::uint24, ebus::nullValue());
    REQUIRE(encoded.size() == 3);
    REQUIRE(encoded[0] == 0xff);
    REQUIRE(encoded[1] == 0xff);
    REQUIRE(encoded[2] == 0xff);
    // raw decode bypasses the sentinel
    auto raw_decoded = ebus::decode(DataType::uint24, raw, Endian::little,
                                    /*use_sentinel=*/false);
    REQUIRE(raw_decoded.has_value());
    REQUIRE(ebus::asInt64(*raw_decoded) == 0xffffff);
  }

  // uint24 out-of-range encode yields an empty sequence
  REQUIRE(
      ebus::encode(DataType::uint24, static_cast<uint32_t>(0x1000000)).empty());

  // uint24r: big-endian byte order on the wire
  {
    Sequence bytes = ebus::encode(DataType::uint24r, static_cast<uint32_t>(414),
                                  Endian::little);
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == 0x00);
    REQUIRE(bytes[1] == 0x01);
    REQUIRE(bytes[2] == 0x9e);
    auto decoded = ebus::decode(DataType::uint24r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == 414);
  }

  // int24 signed roundtrip including negatives
  for (int32_t v = -8388607; v <= 8388607; v += 0x40000) {
    Sequence bytes =
        ebus::encode(DataType::int24, static_cast<int32_t>(v), Endian::little);
    REQUIRE(bytes.size() == 3);
    auto decoded = ebus::decode(DataType::int24, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == v);
  }

  // int24 null sentinel (-8388608 = 0x800000)
  {
    std::vector<uint8_t> raw = {0x00, 0x00, 0x80};
    auto decoded = ebus::decode(DataType::int24, raw);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::isNull(*decoded));
    Sequence encoded = ebus::encode(DataType::int24, ebus::nullValue());
    REQUIRE(encoded.size() == 3);
    REQUIRE(encoded[0] == 0x00);
    REQUIRE(encoded[1] == 0x00);
    REQUIRE(encoded[2] == 0x80);
  }

  // int24 range: 8388608 / -8388609 are out of range
  REQUIRE(ebus::encode(DataType::int24, static_cast<int32_t>(8388608)).empty());
  REQUIRE(
      ebus::encode(DataType::int24, static_cast<int32_t>(-8388609)).empty());

  // int24r: big-endian byte order on the wire
  {
    Sequence bytes = ebus::encode(DataType::int24r, static_cast<int32_t>(-2),
                                  Endian::little);
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == 0xff);
    REQUIRE(bytes[1] == 0xff);
    REQUIRE(bytes[2] == 0xfe);
    auto decoded = ebus::decode(DataType::int24r, bytes, Endian::little);
    REQUIRE(decoded.has_value());
    REQUIRE(ebus::asInt64(*decoded) == -2);
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
    Sequence bytes = ebus::encode(DataType::float32, value, Endian::little);
    auto decoded = ebus::decode(DataType::float32, bytes, Endian::little);
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
    Sequence bytes = ebus::encode(DataType::float32r, value, Endian::little);
    auto decoded = ebus::decode(DataType::float32r, bytes, Endian::little);
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

TEST_CASE("Datatypes: wide hex and char (9..16)", "[models][datatypes]") {
  // Name lookup must stay alphabetical in meta_table (strcmp binary
  // search): HEX10..HEX16 / CHAR10..CHAR16 sort between *1 and *2,
  // HEX9 / CHAR9 after *8.
  REQUIRE(ebus::stringToDataType("HEX9") == DataType::hex9);
  REQUIRE(ebus::stringToDataType("HEX10") == DataType::hex10);
  REQUIRE(ebus::stringToDataType("HEX16") == DataType::hex16);
  REQUIRE(ebus::stringToDataType("HEX2") == DataType::hex2);
  REQUIRE(ebus::stringToDataType("HEX8") == DataType::hex8);
  REQUIRE(ebus::stringToDataType("HEX17") == DataType::error);
  REQUIRE(ebus::stringToDataType("CHAR9") == DataType::char9);
  REQUIRE(ebus::stringToDataType("CHAR10") == DataType::char10);
  REQUIRE(ebus::stringToDataType("CHAR16") == DataType::char16);
  REQUIRE(ebus::stringToDataType("CHAR2") == DataType::char2);
  REQUIRE(ebus::stringToDataType("CHAR8") == DataType::char8);
  REQUIRE(ebus::stringToDataType("CHAR17") == DataType::error);

  REQUIRE(std::string(ebus::dataTypeToString(DataType::hex9)) == "HEX9");
  REQUIRE(std::string(ebus::dataTypeToString(DataType::hex16)) == "HEX16");
  REQUIRE(std::string(ebus::dataTypeToString(DataType::char9)) == "CHAR9");
  REQUIRE(std::string(ebus::dataTypeToString(DataType::char16)) == "CHAR16");

  REQUIRE(ebus::sizeOfDataType(DataType::hex9) == 9);
  REQUIRE(ebus::sizeOfDataType(DataType::hex16) == 16);
  REQUIRE(ebus::sizeOfDataType(DataType::char9) == 9);
  REQUIRE(ebus::sizeOfDataType(DataType::char16) == 16);
  REQUIRE(ebus::isNumeric(DataType::hex9) == false);
  REQUIRE(ebus::isNumeric(DataType::hex16) == false);
  REQUIRE(ebus::isNumeric(DataType::char9) == false);
  REQUIRE(ebus::isNumeric(DataType::char16) == false);

  // Encode/decode roundtrip mirrors the hex8/char8 cases: the value is
  // the literal byte string, padded with NULs up to the type size.
  std::vector<std::string> wide = {"0123456789abcdef", "slot0timer1!!",
                                   "deadbeefdeadbeef"};
  for (const std::string& str : wide) {
    Sequence bytes9 = ebus::encode(DataType::hex9, str);
    REQUIRE(bytes9.size() == 9);
    auto decoded9 = ebus::decode(DataType::hex9, bytes9);
    REQUIRE(decoded9.has_value());
    REQUIRE(std::get<std::string>(*decoded9).find(str.substr(0, 9)) == 0);

    Sequence bytes16 = ebus::encode(DataType::hex16, str);
    REQUIRE(bytes16.size() == 16);
    auto decoded16 = ebus::decode(DataType::hex16, bytes16);
    REQUIRE(decoded16.has_value());
    REQUIRE(std::get<std::string>(*decoded16).find(str.substr(0, 16)) == 0);

    Sequence cbytes9 = ebus::encode(DataType::char9, str);
    REQUIRE(cbytes9.size() == 9);
    auto cdecoded9 = ebus::decode(DataType::char9, cbytes9);
    REQUIRE(cdecoded9.has_value());
    REQUIRE(std::get<std::string>(*cdecoded9).find(str.substr(0, 9)) == 0);

    Sequence cbytes16 = ebus::encode(DataType::char16, str);
    REQUIRE(cbytes16.size() == 16);
    auto cdecoded16 = ebus::decode(DataType::char16, cbytes16);
    REQUIRE(cdecoded16.has_value());
    REQUIRE(std::get<std::string>(*cdecoded16).find(str.substr(0, 16)) == 0);
  }

  // Shorter strings zero-pad to the exact type size.
  Sequence padded = ebus::encode(DataType::hex9, std::string("ab"));
  REQUIRE(padded.size() == 9);
  REQUIRE(padded[2] == 0);
  Sequence cpadded = ebus::encode(DataType::char16, std::string("ab"));
  REQUIRE(cpadded.size() == 16);
  REQUIRE(cpadded[2] == 0);
}

TEST_CASE("Datatypes: STR rendering", "[models][datatypes]") {
  // All-0xFF (unprogrammed EEPROM) renders '?' per byte, never soup.
  auto ff = ebus::decode(DataType::char8,
                         ebus::ByteView(reinterpret_cast<const uint8_t*>(
                                            "\xff\xff\xff\xff\xff\xff\xff\xff"),
                                        8));
  REQUIRE(ff);
  REQUIRE(std::get<std::string>(*ff) == "????????");

  // First NUL terminates; trailing content ignored.
  const uint8_t ab_nul_cd[8] = {'A', 'B', 0x00, 'C', 'D', 0x00, 0x00, 0x00};
  auto nul = ebus::decode(DataType::char8, ebus::ByteView(ab_nul_cd, 8));
  REQUIRE(nul);
  REQUIRE(std::get<std::string>(*nul) == "AB");

  // Control bytes become space, '<' stays literal (printable).
  const uint8_t ctrl[8] = {'3', '0', 0x01, '<', '<', 0x00, 0x00, 0x00};
  auto mixed = ebus::decode(DataType::char8, ebus::ByteView(ctrl, 8));
  REQUIRE(mixed);
  REQUIRE(std::get<std::string>(*mixed) == "30 <<");

  // CHAR1 0xFF is data ('?'), not null: strings have no sentinel mapping.
  auto c1 =
      ebus::decode(DataType::char1,
                   ebus::ByteView(reinterpret_cast<const uint8_t*>("\xff"), 1));
  REQUIRE(c1);
  REQUIRE(!ebus::isNull(*c1));
  REQUIRE(std::get<std::string>(*c1) == "?");
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

  // float32 (default for float)
  DataValue v2 = 21.5f;
  Sequence s2 = ebus::encode(DataType::auto_detect, v2);
  REQUIRE(s2.size() == 4);
  auto d2 = ebus::decode(DataType::float32, s2);
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
  Sequence encoded_fixed =
      ebus::encode(DataType::data2b,
                   static_cast<int64_t>(
                       12.5f * detail::FixedPointLimits::fixed_point_scale));
  REQUIRE(encoded_fixed.size() == 2);
  REQUIRE(encoded_fixed[0] == 0x80);
  REQUIRE(encoded_fixed[1] == 0x0C);

  // Encode 1.5 (as fixed-point int64_t) to DATA1C
  Sequence encoded_fixed_1c = ebus::encode(
      DataType::data1c,
      static_cast<int64_t>(1.5f * detail::FixedPointLimits::fixed_point_scale));
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
  REQUIRE(ebus::isValid(DataType::bcd8,
                        ebus::ByteView(bcd_12.data(), bcd_12.size())));  // "12"
  static const std::array<uint8_t, 1> bcd_1a = {0x1a};
  REQUIRE(!ebus::isValid(
      DataType::bcd8,
      ebus::ByteView(bcd_1a.data(), bcd_1a.size())));  // Invalid nibble
  static const std::array<uint8_t, 1> bcd_ff = {0xff};
  REQUIRE(ebus::isValid(
      DataType::bcd8,
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
  auto v1 = ebus::decode(
      DataType::bcd8,
      ebus::ByteView(bcd_ff_sentinel.data(), bcd_ff_sentinel.size()));
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

TEST_CASE("Datatypes: raw decode skips sentinels", "[models][datatypes]") {
  // Same sentinel bytes, use_sentinel=false: raw numerics, never null.
  auto r1 = ebus::decode(DataType::data1b, ebus::ByteView({0x80}),
                         ebus::Endian::little, false);
  REQUIRE(r1);
  REQUIRE(!ebus::isNull(*r1));
  REQUIRE(ebus::asInt64(*r1) == -128);

  auto r2 = ebus::decode(DataType::uint16, ebus::ByteView({0xff, 0xff}),
                         ebus::Endian::little, false);
  REQUIRE(r2);
  REQUIRE(!ebus::isNull(*r2));
  REQUIRE(ebus::asInt64(*r2) == 65535);

  // Default (omitted flag) still nulls: presentation opt-in only.
  auto d1 = ebus::decode(DataType::data1b, ebus::ByteView({0x80}));
  REQUIRE(d1);
  REQUIRE(ebus::isNull(*d1));
}

TEST_CASE("Datatypes: isNumeric(DataType)", "[models][datatypes]") {
  REQUIRE(ebus::isNumeric(DataType::uint8) == true);
  REQUIRE(ebus::isNumeric(DataType::int8) == true);
  REQUIRE(ebus::isNumeric(DataType::float32) == true);
  REQUIRE(ebus::isNumeric(DataType::data1b) == true);
  REQUIRE(ebus::isNumeric(DataType::bcd8) == true);
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
  Sequence bcd_null = ebus::encode(DataType::bcd8, ebus::nullValue());
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

  // float32 has no replacement_value, so encoding null should return
  // an empty sequence.
  Sequence float32_null = ebus::encode(DataType::float32, ebus::nullValue());
  REQUIRE(float32_null.empty());

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
TEST_CASE("Datatypes: date_bcd4", "[models][datatypes]") {
  // 17.03.2021 (Wed) from wire 17 03 03 21.
  const uint8_t bda_2021[4] = {0x17, 0x03, 0x03, 0x21};
  auto d1 = ebus::decode(DataType::date_bcd4, ByteView(bda_2021, 4));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "17.03.2021");

  // 30.11.2024 (Sat) from wire 30 11 06 24.
  const uint8_t bda_2024[4] = {0x30, 0x11, 0x06, 0x24};
  auto d2 = ebus::decode(DataType::date_bcd4, ByteView(bda_2024, 4));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "30.11.2024");

  // Write computes the weekday byte from the date.
  Sequence enc1 = ebus::encode(DataType::date_bcd4, std::string("17.03.2021"));
  REQUIRE(enc1.size() == 4);
  REQUIRE(enc1[0] == 0x17);
  REQUIRE(enc1[1] == 0x03);
  REQUIRE(enc1[2] == 0x03);
  REQUIRE(enc1[3] == 0x21);

  Sequence enc2 = ebus::encode(DataType::date_bcd4, std::string("30.11.2024"));
  REQUIRE(enc2.size() == 4);
  REQUIRE(enc2[0] == 0x30);
  REQUIRE(enc2[1] == 0x11);
  REQUIRE(enc2[2] == 0x06);
  REQUIRE(enc2[3] == 0x24);

  // Year byte 0x00 = 2000; unpadded input accepted.
  const uint8_t bda_2000[4] = {0x01, 0x01, 0x01, 0x00};
  auto d3 = ebus::decode(DataType::date_bcd4, ByteView(bda_2000, 4));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "01.01.2000");
  Sequence enc3 = ebus::encode(DataType::date_bcd4, std::string("1.1.2000"));
  REQUIRE(enc3.size() == 4);
  REQUIRE(enc3[0] == 0x01);
  REQUIRE(enc3[3] == 0x00);

  // Replacement bytes render the whole field null (whole-field, never a
  // partial string like "null.03.2021").
  const uint8_t bda_ff[4] = {0xff, 0xff, 0xff, 0xff};
  auto d4 = ebus::decode(DataType::date_bcd4, ByteView(bda_ff, 4));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  const uint8_t bda_weekday_ff[4] = {0x17, 0x03, 0xff, 0x21};
  auto d5 = ebus::decode(DataType::date_bcd4, ByteView(bda_weekday_ff, 4));
  REQUIRE(d5);
  REQUIRE(ebus::isNull(*d5));
  // Raw mode skips the sentinel gate; the weekday byte is ignored anyway.
  auto d6 = ebus::decode(DataType::date_bcd4, ByteView(bda_weekday_ff, 4),
                         Endian::little, false);
  REQUIRE(d6);
  REQUIRE(std::get<std::string>(*d6) == "17.03.2021");

  // Zero day/month are null markers, not protocol errors.
  const uint8_t bda_day0[4] = {0x00, 0x03, 0x03, 0x21};
  auto d7 = ebus::decode(DataType::date_bcd4, ByteView(bda_day0, 4));
  REQUIRE(d7);
  REQUIRE(ebus::isNull(*d7));

  // Out-of-range or invalid BCD: protocol errors -> nullopt.
  const uint8_t bda_day32[4] = {0x32, 0x03, 0x03, 0x21};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd4, ByteView(bda_day32, 4)).has_value());
  const uint8_t bda_month13[4] = {0x17, 0x13, 0x03, 0x21};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd4, ByteView(bda_month13, 4)).has_value());
  const uint8_t bda_bad_bcd[4] = {0x1a, 0x03, 0x03, 0x21};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd4, ByteView(bda_bad_bcd, 4)).has_value());

  // Invalid write input -> empty sequence.
  REQUIRE(ebus::encode(DataType::date_bcd4, std::string("32.03.2021")).empty());
  REQUIRE(ebus::encode(DataType::date_bcd4, std::string("17.13.2021")).empty());
  REQUIRE(ebus::encode(DataType::date_bcd4, std::string("17.03.1999")).empty());

  // Null encodes to the wire replacement.
  Sequence enc_null = ebus::encode(DataType::date_bcd4, ebus::nullValue());
  REQUIRE(enc_null.size() == 4);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[3] == 0xff);

  // isValid: replacement and zero-day are valid null results; bad BCD is not.
  const uint8_t zeros[4] = {0, 0, 0, 0};
  REQUIRE(ebus::isValid(DataType::date_bcd4, ByteView(zeros, 4)));
  REQUIRE(ebus::isValid(DataType::date_bcd4, ByteView(bda_ff, 4)));
  REQUIRE_FALSE(ebus::isValid(DataType::date_bcd4, ByteView(bda_bad_bcd, 4)));
}

TEST_CASE("Datatypes: date_bin3", "[models][datatypes]") {
  // Plain 3-byte date, no weekday byte: bytes are plain decimal (0x11=17).
  const uint8_t date_bin3_2021[3] = {0x11, 0x03, 0x15};
  auto d1 = ebus::decode(DataType::date_bin3, ByteView(date_bin3_2021, 3));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "17.03.2021");
  Sequence enc1 = ebus::encode(DataType::date_bin3, std::string("17.03.2021"));
  REQUIRE(enc1.size() == 3);
  REQUIRE(enc1[0] == 0x11);
  REQUIRE(enc1[1] == 0x03);
  REQUIRE(enc1[2] == 0x15);

  // Spec upper bound 31.12.2099 (0x1f, 0x0c, 0x63).
  const uint8_t hda3_max[3] = {0x1f, 0x0c, 0x63};
  auto d2 = ebus::decode(DataType::date_bin3, ByteView(hda3_max, 3));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "31.12.2099");

  // Plain year byte above 99 is outside the spec range (0x00..0x63).
  const uint8_t hda3_bad_year[3] = {0x11, 0x03, 0xc8};
  REQUIRE_FALSE(ebus::decode(DataType::date_bin3, ByteView(hda3_bad_year, 3))
                    .has_value());

  // Replacement -> null; zero day -> null.
  const uint8_t hda3_ff[3] = {0xff, 0xff, 0xff};
  auto d3 = ebus::decode(DataType::date_bin3, ByteView(hda3_ff, 3));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  const uint8_t hda3_day0[3] = {0x00, 0x03, 0x15};
  auto d4 = ebus::decode(DataType::date_bin3, ByteView(hda3_day0, 3));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));

  Sequence enc_null = ebus::encode(DataType::date_bin3, ebus::nullValue());
  REQUIRE(enc_null.size() == 3);
  REQUIRE(enc_null[0] == 0xff);
}

TEST_CASE("Datatypes: time_bcd3r", "[models][datatypes]") {
  // 07:42:16 from wire 16 42 07.
  const uint8_t bti1[3] = {0x16, 0x42, 0x07};
  auto d1 = ebus::decode(DataType::time_bcd3r, ByteView(bti1, 3));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "07:42:16");

  // 21:05:33 from wire 33 05 21.
  const uint8_t bti2[3] = {0x33, 0x05, 0x21};
  auto d2 = ebus::decode(DataType::time_bcd3r, ByteView(bti2, 3));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "21:05:33");

  Sequence enc1 = ebus::encode(DataType::time_bcd3r, std::string("07:42:16"));
  REQUIRE(enc1.size() == 3);
  REQUIRE(enc1[0] == 0x16);
  REQUIRE(enc1[1] == 0x42);
  REQUIRE(enc1[2] == 0x07);

  // 24:00:00 is accepted (hour byte BCD 0x24); 24:00:01 is not.
  const uint8_t bti_2400[3] = {0x00, 0x00, 0x24};
  auto d3 = ebus::decode(DataType::time_bcd3r, ByteView(bti_2400, 3));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "24:00:00");
  const uint8_t bti_2401[3] = {0x01, 0x00, 0x24};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd3r, ByteView(bti_2401, 3)).has_value());

  // Range and BCD errors (minute byte BCD 0x60 = 60).
  const uint8_t bti_min60[3] = {0x00, 0x60, 0x07};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd3r, ByteView(bti_min60, 3)).has_value());
  const uint8_t bti_hour25[3] = {0x00, 0x00, 0x25};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd3r, ByteView(bti_hour25, 3)).has_value());
  const uint8_t bti_bad_bcd[3] = {0x16, 0x42, 0x0a};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd3r, ByteView(bti_bad_bcd, 3)).has_value());

  // Replacement -> null; zeros -> 00:00:00.
  const uint8_t bti_ff[3] = {0xff, 0xff, 0xff};
  auto d4 = ebus::decode(DataType::time_bcd3r, ByteView(bti_ff, 3));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  const uint8_t bti_zero[3] = {0x00, 0x00, 0x00};
  auto d5 = ebus::decode(DataType::time_bcd3r, ByteView(bti_zero, 3));
  REQUIRE(d5);
  REQUIRE(std::get<std::string>(*d5) == "00:00:00");

  Sequence enc2 = ebus::encode(DataType::time_bcd3r, std::string("24:00:00"));
  REQUIRE(enc2.size() == 3);
  REQUIRE(enc2[2] == 0x24);
  REQUIRE(ebus::encode(DataType::time_bcd3r, std::string("24:01:00")).empty());
}

TEST_CASE("Datatypes: time_bin3r", "[models][datatypes]") {
  // Vaillant layout: wire SS MM HH, plain binary, replacement 0x63.
  const uint8_t vti[3] = {0x3b, 0x3b, 0x17};
  auto d1 = ebus::decode(DataType::time_bin3r, ByteView(vti, 3));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "23:59:59");
  Sequence enc1 = ebus::encode(DataType::time_bin3r, std::string("23:59:59"));
  REQUIRE(enc1.size() == 3);
  REQUIRE(enc1[0] == 0x3b);
  REQUIRE(enc1[1] == 0x3b);
  REQUIRE(enc1[2] == 0x17);

  // Per-byte replacement 0x63 nulls the field in any position.
  const uint8_t vti_repl0[3] = {0x63, 0x3b, 0x17};
  auto d2 = ebus::decode(DataType::time_bin3r, ByteView(vti_repl0, 3));
  REQUIRE(d2);
  REQUIRE(ebus::isNull(*d2));
  const uint8_t vti_repl2[3] = {0x3b, 0x3b, 0x63};
  auto d3 = ebus::decode(DataType::time_bin3r, ByteView(vti_repl2, 3));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));

  // Null encodes to the per-byte replacement (0x63), not 0xff.
  Sequence enc_null = ebus::encode(DataType::time_bin3r, ebus::nullValue());
  REQUIRE(enc_null.size() == 3);
  REQUIRE(enc_null[0] == 0x63);
  REQUIRE(enc_null[2] == 0x63);
}

TEST_CASE("Datatypes: time_bin2r", "[models][datatypes]") {
  // Wire MM HH, plain binary.
  const uint8_t vtm[2] = {0x3b, 0x17};
  auto d1 = ebus::decode(DataType::time_bin2r, ByteView(vtm, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "23:59");
  Sequence enc1 = ebus::encode(DataType::time_bin2r, std::string("23:59"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0x3b);
  REQUIRE(enc1[1] == 0x17);

  // 24:00 allowed with zero minutes.
  const uint8_t vtm_2400[2] = {0x00, 0x18};
  auto d2 = ebus::decode(DataType::time_bin2r, ByteView(vtm_2400, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "24:00");
  REQUIRE(ebus::encode(DataType::time_bin2r, std::string("24:01")).empty());

  // Replacement -> null.
  const uint8_t vtm_ff[2] = {0xff, 0xff};
  auto d3 = ebus::decode(DataType::time_bin2r, ByteView(vtm_ff, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
}

TEST_CASE("Datatypes: time_min2", "[models][datatypes]") {
  // Minutes since midnight, little-endian: 12:30 = 750 = 0x02ee.
  Sequence enc1 = ebus::encode(DataType::time_min2, std::string("12:30"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0xee);
  REQUIRE(enc1[1] == 0x02);
  auto d1 = ebus::decode(DataType::time_min2, ByteView(enc1.data(), 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "12:30");

  // Upper bound 24:00 = 1440 = 0x05a0.
  Sequence enc2 = ebus::encode(DataType::time_min2, std::string("24:00"));
  REQUIRE(enc2.size() == 2);
  REQUIRE(enc2[0] == 0xa0);
  REQUIRE(enc2[1] == 0x05);
  auto d2 = ebus::decode(DataType::time_min2, ByteView(enc2.data(), 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "24:00");

  // 1441 minutes is out of range.
  const uint8_t min_1441[2] = {0xa1, 0x05};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_min2, ByteView(min_1441, 2)).has_value());

  // Replacement: a single 0xff byte nulls the field (0xff,0x00 would be
  // 255 minutes = 04:15, inside the numeric range).
  const uint8_t min_repl[2] = {0xff, 0x00};
  auto d3 = ebus::decode(DataType::time_min2, ByteView(min_repl, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  auto d4 = ebus::decode(DataType::time_min2, ByteView(min_repl, 2),
                         Endian::little, false);
  REQUIRE(d4);
  REQUIRE(std::get<std::string>(*d4) == "04:15");

  Sequence enc_null = ebus::encode(DataType::time_min2, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
}

TEST_CASE("Datatypes: time_trunc10", "[models][datatypes]") {
  // minutes div 10 + hour * 6: 29 = 04:50.
  const uint8_t ttm_29[1] = {29};
  auto d1 = ebus::decode(DataType::time_trunc10, ByteView(ttm_29, 1));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "04:50");
  Sequence enc1 = ebus::encode(DataType::time_trunc10, std::string("04:50"));
  REQUIRE(enc1.size() == 1);
  REQUIRE(enc1[0] == 29);

  // Upper valid step 143 = 23:50; zero = 00:00.
  const uint8_t ttm_143[1] = {143};
  auto d2 = ebus::decode(DataType::time_trunc10, ByteView(ttm_143, 1));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "23:50");
  const uint8_t ttm_0[1] = {0};
  auto d3 = ebus::decode(DataType::time_trunc10, ByteView(ttm_0, 1));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "00:00");

  // 0x90 is the replacement: null with sentinels, 24:00 raw.
  const uint8_t ttm_repl[1] = {0x90};
  auto d4 = ebus::decode(DataType::time_trunc10, ByteView(ttm_repl, 1));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  auto d5 = ebus::decode(DataType::time_trunc10, ByteView(ttm_repl, 1),
                         Endian::little, false);
  REQUIRE(d5);
  REQUIRE(std::get<std::string>(*d5) == "24:00");

  // 145 = 24:10 and 150 = 25:00 are invalid (hour/min consistency).
  const uint8_t ttm_145[1] = {145};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc10, ByteView(ttm_145, 1)).has_value());
  const uint8_t ttm_150[1] = {150};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc10, ByteView(ttm_150, 1)).has_value());

  // Write rounds to the nearest step: 23:57 -> 144 = 0x90, which reads
  // back as null.
  Sequence enc2 = ebus::encode(DataType::time_trunc10, std::string("23:57"));
  REQUIRE(enc2.size() == 1);
  REQUIRE(enc2[0] == 0x90);

  Sequence enc_null = ebus::encode(DataType::time_trunc10, ebus::nullValue());
  REQUIRE(enc_null.size() == 1);
  REQUIRE(enc_null[0] == 0x90);
}

TEST_CASE("Datatypes: pin2", "[models][datatypes]") {
  // Wire is big-endian BCD digits; output is zero-padded to 4 digits.
  const uint8_t pin_77[2] = {0x00, 0x77};
  auto d1 = ebus::decode(DataType::pin2, ByteView(pin_77, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "0077");
  Sequence enc1 = ebus::encode(DataType::pin2, std::string("0077"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0x00);
  REQUIRE(enc1[1] == 0x77);

  // Unpadded input parses to the same value.
  Sequence enc2 = ebus::encode(DataType::pin2, std::string("77"));
  REQUIRE(enc2.size() == 2);
  REQUIRE(enc2[0] == 0x00);
  REQUIRE(enc2[1] == 0x77);

  const uint8_t pin_1234[2] = {0x12, 0x34};
  auto d2 = ebus::decode(DataType::pin2, ByteView(pin_1234, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "1234");

  // Replacement 0xffff -> null; invalid BCD nibbles -> protocol error.
  const uint8_t pin_ff[2] = {0xff, 0xff};
  auto d3 = ebus::decode(DataType::pin2, ByteView(pin_ff, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  const uint8_t pin_bad[2] = {0x1a, 0x23};
  REQUIRE_FALSE(ebus::decode(DataType::pin2, ByteView(pin_bad, 2)).has_value());

  // Write validation: max 9999, digits only.
  REQUIRE(ebus::encode(DataType::pin2, std::string("10000")).empty());
  REQUIRE(ebus::encode(DataType::pin2, std::string("abcd")).empty());

  // Null (variant or the "null" token) encodes to the replacement.
  Sequence enc_null = ebus::encode(DataType::pin2, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
  Sequence enc_token = ebus::encode(DataType::pin2, std::string("null"));
  REQUIRE(enc_token.size() == 2);
  REQUIRE(enc_token[0] == 0xff);
}

TEST_CASE("Datatypes: tem_p", "[models][datatypes]") {
  // Slave-side parameter id "GG-NNN": the little-endian word is packed
  // xxxx GGGG GNNN NNNN (grp in bits 7..11, num in bits 0..6), sentinel
  // 0xffff -> null. The read-side masks keep grp <= 31 and num <= 127.
  const uint8_t p_05023[2] = {0x97, 0x02};  // (5 << 7) | 23 = 0x0297
  auto d1 = ebus::decode(DataType::tem_p, ByteView(p_05023, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "05-023");

  const uint8_t p_00001[2] = {0x01, 0x00};
  auto d2 = ebus::decode(DataType::tem_p, ByteView(p_00001, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "00-001");

  // Field-width maximum (grp 31, num 127 = 0x0fff): a lone 0xff byte
  // is valid data (only the full word is the sentinel).
  const uint8_t p_31127[2] = {0xff, 0x0f};
  auto d3 = ebus::decode(DataType::tem_p, ByteView(p_31127, 2));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "31-127");
  REQUIRE(ebus::isValid(DataType::tem_p, ByteView(p_31127, 2)));

  // Sentinel 0xffff -> null; with use_sentinel=false it decodes raw.
  const uint8_t p_null[2] = {0xff, 0xff};
  auto d4 = ebus::decode(DataType::tem_p, ByteView(p_null, 2));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  auto d5 = ebus::decode(DataType::tem_p, ByteView(p_null, 2),
                         ebus::Endian::little, false);
  REQUIRE(d5);
  REQUIRE(std::get<std::string>(*d5) == "31-127");

  // Roundtrip plus write range checks: grp <= 31, num <= 127 (values
  // beyond the 5-bit / 7-bit wire fields are rejected on write).
  Sequence enc1 = ebus::encode(DataType::tem_p, std::string("05-023"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0x97);
  REQUIRE(enc1[1] == 0x02);
  Sequence enc2 = ebus::encode(DataType::tem_p, std::string("31-127"));
  REQUIRE(enc2.size() == 2);
  REQUIRE(enc2[0] == 0xff);
  REQUIRE(enc2[1] == 0x0f);
  REQUIRE(ebus::encode(DataType::tem_p, std::string("32-000")).empty());
  REQUIRE(ebus::encode(DataType::tem_p, std::string("00-128")).empty());
  REQUIRE(ebus::encode(DataType::tem_p, std::string("abcd")).empty());
  REQUIRE(ebus::encode(DataType::tem_p, std::string("5")).empty());

  // Null (variant, "null" token, or "-" token) -> 0xffff.
  Sequence enc_null = ebus::encode(DataType::tem_p, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[1] == 0xff);
  Sequence enc_token = ebus::encode(DataType::tem_p, std::string("null"));
  REQUIRE(enc_token.size() == 2);
  REQUIRE(enc_token[0] == 0xff);
  Sequence enc_dash = ebus::encode(DataType::tem_p, std::string("-"));
  REQUIRE(enc_dash.size() == 2);
  REQUIRE(enc_dash[0] == 0xff);
  REQUIRE(enc_dash[1] == 0xff);

  REQUIRE(ebus::stringToDataType("TEM_P") == DataType::tem_p);
}

TEST_CASE("Datatypes: date_bcd3", "[models][datatypes]") {
  // DD MM YY BCD, no weekday byte, replacement 0xff.
  const uint8_t bda3[3] = {0x04, 0x09, 0x26};  // 04.09.2026
  auto d1 = ebus::decode(DataType::date_bcd3, ByteView(bda3, 3));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "04.09.2026");
  Sequence enc1 = ebus::encode(DataType::date_bcd3, std::string("04.09.2026"));
  REQUIRE(enc1.size() == 3);
  REQUIRE(enc1[0] == 0x04);
  REQUIRE(enc1[1] == 0x09);
  REQUIRE(enc1[2] == 0x26);

  // Replacement byte in any position -> null; zero day -> null.
  const uint8_t bda3_ff[3] = {0xff, 0x09, 0x26};
  auto d2 = ebus::decode(DataType::date_bcd3, ByteView(bda3_ff, 3));
  REQUIRE(d2);
  REQUIRE(ebus::isNull(*d2));
  const uint8_t bda3_day0[3] = {0x00, 0x09, 0x26};
  auto d3 = ebus::decode(DataType::date_bcd3, ByteView(bda3_day0, 3));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));

  // Invalid BCD nibble / out-of-range day -> protocol error.
  const uint8_t bda3_bad_bcd[3] = {0x04, 0x0a, 0x26};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd3, ByteView(bda3_bad_bcd, 3)).has_value());
  const uint8_t bda3_day32[3] = {0x32, 0x09, 0x26};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd3, ByteView(bda3_day32, 3)).has_value());

  // Invalid write input -> empty sequence.
  REQUIRE(ebus::encode(DataType::date_bcd3, std::string("32.09.2026")).empty());
  REQUIRE(ebus::encode(DataType::date_bcd3, std::string("04.13.2026")).empty());

  // Null (variant or token) -> the wire replacement.
  Sequence enc_null = ebus::encode(DataType::date_bcd3, ebus::nullValue());
  REQUIRE(enc_null.size() == 3);
  REQUIRE(enc_null[0] == 0xff);
  Sequence enc_token = ebus::encode(DataType::date_bcd3, std::string("null"));
  REQUIRE(enc_token.size() == 3);
  REQUIRE(enc_token[0] == 0xff);
}

TEST_CASE("Datatypes: date_bcd4z", "[models][datatypes]") {
  // DD MM WZ YY BCD with SPE weekday: Mon=0x00 .. Sun=0x06.
  const uint8_t bdz_tue[4] = {0x12, 0x08, 0x01, 0x25};  // 12.08.2025 Tue
  auto d1 = ebus::decode(DataType::date_bcd4z, ByteView(bdz_tue, 4));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "12.08.2025");

  // Encode recomputes the weekday (Sunday -> 0x06).
  Sequence enc1 = ebus::encode(DataType::date_bcd4z, std::string("10.08.2025"));
  REQUIRE(enc1.size() == 4);
  REQUIRE(enc1[0] == 0x10);
  REQUIRE(enc1[1] == 0x08);
  REQUIRE(enc1[2] == 0x06);
  REQUIRE(enc1[3] == 0x25);

  // Raw weekday byte is ignored on read.
  const uint8_t bdz_ww[4] = {0x12, 0x08, 0xee, 0x25};
  auto d2 = ebus::decode(DataType::date_bcd4z, ByteView(bdz_ww, 4));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "12.08.2025");

  // Replacement -> null; zero day -> null; bad BCD -> protocol error.
  const uint8_t bdz_ff[4] = {0x12, 0x08, 0x01, 0xff};
  auto d3 = ebus::decode(DataType::date_bcd4z, ByteView(bdz_ff, 4));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  const uint8_t bdz_day0[4] = {0x00, 0x08, 0x01, 0x25};
  auto d4 = ebus::decode(DataType::date_bcd4z, ByteView(bdz_day0, 4));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  const uint8_t bdz_bad[4] = {0x12, 0x0a, 0x01, 0x25};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bcd4z, ByteView(bdz_bad, 4)).has_value());

  // Null -> wire replacement.
  Sequence enc_null = ebus::encode(DataType::date_bcd4z, ebus::nullValue());
  REQUIRE(enc_null.size() == 4);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[3] == 0xff);
}

TEST_CASE("Datatypes: date_bin4", "[models][datatypes]") {
  // DD MM WW YY plain; WW Mon=1..Sun=7, skipped on read.
  const uint8_t hda[4] = {0x0c, 0x08, 0x02, 0x19};  // 12.08.2025 Tue
  auto d1 = ebus::decode(DataType::date_bin4, ByteView(hda, 4));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "12.08.2025");

  // Encode computes the weekday (Sunday -> 7).
  Sequence enc1 = ebus::encode(DataType::date_bin4, std::string("10.08.2025"));
  REQUIRE(enc1.size() == 4);
  REQUIRE(enc1[0] == 0x0a);
  REQUIRE(enc1[1] == 0x08);
  REQUIRE(enc1[2] == 0x07);
  REQUIRE(enc1[3] == 0x19);

  // Garbage weekday byte is ignored on read.
  const uint8_t hda_ww[4] = {0x0c, 0x08, 0xee, 0x19};
  auto d2 = ebus::decode(DataType::date_bin4, ByteView(hda_ww, 4));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "12.08.2025");

  // Replacement -> null; zero day -> null; year byte > 99 -> error.
  const uint8_t hda_ff[4] = {0xff, 0x08, 0x02, 0x19};
  auto d3 = ebus::decode(DataType::date_bin4, ByteView(hda_ff, 4));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  const uint8_t hda_day0[4] = {0x00, 0x08, 0x02, 0x19};
  auto d4 = ebus::decode(DataType::date_bin4, ByteView(hda_day0, 4));
  REQUIRE(d4);
  REQUIRE(ebus::isNull(*d4));
  const uint8_t hda_bad_year[4] = {0x0c, 0x08, 0x02, 0xc8};
  REQUIRE_FALSE(
      ebus::decode(DataType::date_bin4, ByteView(hda_bad_year, 4)).has_value());

  // Null -> wire replacement.
  Sequence enc_null = ebus::encode(DataType::date_bin4, ebus::nullValue());
  REQUIRE(enc_null.size() == 4);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[3] == 0xff);
}

TEST_CASE("Datatypes: date_day2", "[models][datatypes]") {
  // LE u16 days since 01.01.1900; zero is a real date (no null marker).
  const uint8_t day2[2] = {0xbb, 0xb4};  // 46267 -> 04.09.2026
  auto d1 = ebus::decode(DataType::date_day2, ByteView(day2, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "04.09.2026");
  Sequence enc1 = ebus::encode(DataType::date_day2, std::string("04.09.2026"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0xbb);
  REQUIRE(enc1[1] == 0xb4);

  // Wire zero decodes to 01.01.1900 (the read must not shift early MJDs).
  const uint8_t day2_zero[2] = {0x00, 0x00};
  auto d2 = ebus::decode(DataType::date_day2, ByteView(day2_zero, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "01.01.1900");

  // A 0xff byte anywhere nulls the whole field.
  const uint8_t day2_ff[2] = {0xbb, 0xff};
  auto d3 = ebus::decode(DataType::date_day2, ByteView(day2_ff, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));

  // Encode rejects pre-1900 years and day counts beyond u16.
  REQUIRE(ebus::encode(DataType::date_day2, std::string("31.12.1899")).empty());
  REQUIRE(ebus::encode(DataType::date_day2, std::string("31.12.2099")).empty());

  // Null (variant or token) -> wire replacement bytes.
  Sequence enc_null = ebus::encode(DataType::date_day2, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[1] == 0xff);
  Sequence enc_token = ebus::encode(DataType::date_day2, std::string("null"));
  REQUIRE(enc_token.size() == 2);
  REQUIRE(enc_token[0] == 0xff);
}

TEST_CASE("Datatypes: datetime_min4", "[models][datatypes]") {
  // LE u32 minutes since 01.01.2009, rendered "dd.MM.yyyy HH:mm";
  // required field (REQ): no wire replacement exists.
  const uint8_t dtm[4] = {0xed, 0x06, 0x7c, 0x00};  // -> 15.06.2024 14:37
  auto d1 = ebus::decode(DataType::datetime_min4, ByteView(dtm, 4));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "15.06.2024 14:37");
  Sequence enc1 =
      ebus::encode(DataType::datetime_min4, std::string("15.06.2024 14:37"));
  REQUIRE(enc1.size() == 4);
  REQUIRE(enc1[0] == 0xed);
  REQUIRE(enc1[1] == 0x06);
  REQUIRE(enc1[2] == 0x7c);
  REQUIRE(enc1[3] == 0x00);

  // Wire zero -> the epoch 01.01.2009 00:00.
  const uint8_t dtm_zero[4] = {0, 0, 0, 0};
  auto d2 = ebus::decode(DataType::datetime_min4, ByteView(dtm_zero, 4));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "01.01.2009 00:00");

  // Upper bound 31.12.2099 23:59 decodes; one minute beyond is invalid.
  const uint8_t dtm_max[4] = {0x1f, 0x4e, 0xda, 0x02};
  auto d3 = ebus::decode(DataType::datetime_min4, ByteView(dtm_max, 4));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "31.12.2099 23:59");
  const uint8_t dtm_over[4] = {0x20, 0x4e, 0xda, 0x02};
  REQUIRE_FALSE(
      ebus::decode(DataType::datetime_min4, ByteView(dtm_over, 4)).has_value());

  // Write rejects pre-epoch dates, malformed input, and "null": a required
  // field has no representable null.
  REQUIRE(ebus::encode(DataType::datetime_min4, std::string("01.01.2008 00:00"))
              .empty());
  REQUIRE(
      ebus::encode(DataType::datetime_min4, std::string("15.06.2024")).empty());
  REQUIRE(
      ebus::encode(DataType::datetime_min4, std::string("15.06.2024 14:37:00"))
          .empty());
  REQUIRE(ebus::encode(DataType::datetime_min4, std::string("null")).empty());
  REQUIRE(ebus::encode(DataType::datetime_min4, ebus::nullValue()).empty());

  // isValid: zero and max are valid, one minute beyond is not.
  REQUIRE(ebus::isValid(DataType::datetime_min4, ByteView(dtm_zero, 4)));
  REQUIRE(ebus::isValid(DataType::datetime_min4, ByteView(dtm_max, 4)));
  REQUIRE_FALSE(ebus::isValid(DataType::datetime_min4, ByteView(dtm_over, 4)));
}

TEST_CASE("Datatypes: time_bcd2r", "[models][datatypes]") {
  // MM HH BCD, REV layout, replacement 0xff; no seconds.
  const uint8_t btm[2] = {0x37, 0x14};  // 14:37
  auto d1 = ebus::decode(DataType::time_bcd2r, ByteView(btm, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "14:37");
  Sequence enc1 = ebus::encode(DataType::time_bcd2r, std::string("14:37"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0x37);
  REQUIRE(enc1[1] == 0x14);

  // 24:00 with zero minutes is valid; 24:01 is not.
  const uint8_t btm_2400[2] = {0x00, 0x24};
  auto d2 = ebus::decode(DataType::time_bcd2r, ByteView(btm_2400, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "24:00");
  const uint8_t btm_2401[2] = {0x01, 0x24};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd2r, ByteView(btm_2401, 2)).has_value());

  // Bad BCD nibble / minute 60 -> protocol error.
  const uint8_t btm_bad_bcd[2] = {0x37, 0x1a};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd2r, ByteView(btm_bad_bcd, 2)).has_value());
  const uint8_t btm_min60[2] = {0x60, 0x14};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bcd2r, ByteView(btm_min60, 2)).has_value());

  // Replacement in either byte -> null; null encodes back to it.
  const uint8_t btm_ff[2] = {0xff, 0x14};
  auto d3 = ebus::decode(DataType::time_bcd2r, ByteView(btm_ff, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  Sequence enc_null = ebus::encode(DataType::time_bcd2r, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[1] == 0xff);

  // Invalid write input -> empty sequence.
  REQUIRE(ebus::encode(DataType::time_bcd2r, std::string("25:00")).empty());
  REQUIRE(ebus::encode(DataType::time_bcd2r, std::string("14:60")).empty());
}

TEST_CASE("Datatypes: time_bin2", "[models][datatypes]") {
  // HH MM plain, forward layout, replacement 0xff.
  const uint8_t htm[2] = {0x0e, 0x25};  // 14:37
  auto d1 = ebus::decode(DataType::time_bin2, ByteView(htm, 2));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "14:37");
  Sequence enc1 = ebus::encode(DataType::time_bin2, std::string("14:37"));
  REQUIRE(enc1.size() == 2);
  REQUIRE(enc1[0] == 0x0e);
  REQUIRE(enc1[1] == 0x25);

  // 24:00 valid; 24:01, minute 60, hour 25 invalid.
  const uint8_t htm_2400[2] = {0x18, 0x00};
  auto d2 = ebus::decode(DataType::time_bin2, ByteView(htm_2400, 2));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "24:00");
  const uint8_t htm_2401[2] = {0x18, 0x01};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bin2, ByteView(htm_2401, 2)).has_value());
  const uint8_t htm_min60[2] = {0x0e, 0x3c};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bin2, ByteView(htm_min60, 2)).has_value());
  const uint8_t htm_hour25[2] = {0x19, 0x37};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bin2, ByteView(htm_hour25, 2)).has_value());

  // Replacement -> null; null (variant and token) -> replacement.
  const uint8_t htm_ff[2] = {0xff, 0x25};
  auto d3 = ebus::decode(DataType::time_bin2, ByteView(htm_ff, 2));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  Sequence enc_null = ebus::encode(DataType::time_bin2, ebus::nullValue());
  REQUIRE(enc_null.size() == 2);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[1] == 0xff);
  Sequence enc_token = ebus::encode(DataType::time_bin2, std::string("null"));
  REQUIRE(enc_token.size() == 2);
  REQUIRE(enc_token[0] == 0xff);
}

TEST_CASE("Datatypes: time_bin3", "[models][datatypes]") {
  // HH MM SS plain, forward layout, replacement 0xff.
  const uint8_t hti[3] = {0x0e, 0x25, 0x09};  // 14:37:09
  auto d1 = ebus::decode(DataType::time_bin3, ByteView(hti, 3));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "14:37:09");
  Sequence enc1 = ebus::encode(DataType::time_bin3, std::string("14:37:09"));
  REQUIRE(enc1.size() == 3);
  REQUIRE(enc1[0] == 0x0e);
  REQUIRE(enc1[1] == 0x25);
  REQUIRE(enc1[2] == 0x09);

  // 24:00:00 valid; 24:00:01 and second 60 invalid.
  const uint8_t hti_2400[3] = {0x18, 0x00, 0x00};
  auto d2 = ebus::decode(DataType::time_bin3, ByteView(hti_2400, 3));
  REQUIRE(d2);
  REQUIRE(std::get<std::string>(*d2) == "24:00:00");
  const uint8_t hti_240001[3] = {0x18, 0x00, 0x01};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bin3, ByteView(hti_240001, 3)).has_value());
  const uint8_t hti_sec60[3] = {0x0e, 0x25, 0x3c};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_bin3, ByteView(hti_sec60, 3)).has_value());

  // Replacement in any byte -> null; null -> replacement.
  const uint8_t hti_ff[3] = {0x0e, 0xff, 0x09};
  auto d3 = ebus::decode(DataType::time_bin3, ByteView(hti_ff, 3));
  REQUIRE(d3);
  REQUIRE(ebus::isNull(*d3));
  Sequence enc_null = ebus::encode(DataType::time_bin3, ebus::nullValue());
  REQUIRE(enc_null.size() == 3);
  REQUIRE(enc_null[0] == 0xff);
  REQUIRE(enc_null[2] == 0xff);

  // Seconds are required for this type.
  REQUIRE(ebus::encode(DataType::time_bin3, std::string("14:37")).empty());
}

TEST_CASE("Datatypes: time_trunc15", "[models][datatypes]") {
  // 1 byte, low 7 bits, 15-minute steps, wire replacement 0x00.
  const uint8_t ttq[1] = {0x1d};  // 29 steps -> 07:15
  auto d1 = ebus::decode(DataType::time_trunc15, ByteView(ttq, 1));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "07:15");

  // Write rounds to the nearest step: 07:10 -> 07:15 (still 29).
  Sequence enc1 = ebus::encode(DataType::time_trunc15, std::string("07:10"));
  REQUIRE(enc1.size() == 1);
  REQUIRE(enc1[0] == 0x1d);

  // Wire zero is the replacement -> null; bit 7 is masked off on read.
  const uint8_t ttq_zero[1] = {0x00};
  auto d2 = ebus::decode(DataType::time_trunc15, ByteView(ttq_zero, 1));
  REQUIRE(d2);
  REQUIRE(ebus::isNull(*d2));
  const uint8_t ttq_bit7[1] = {0x40};  // 64 -> 16:00
  auto d3 = ebus::decode(DataType::time_trunc15, ByteView(ttq_bit7, 1));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "16:00");

  // 24:00 fits the step range; 24:15 and 31:15 do not decode.
  const uint8_t ttq_2400[1] = {0x60};
  auto d4 = ebus::decode(DataType::time_trunc15, ByteView(ttq_2400, 1));
  REQUIRE(d4);
  REQUIRE(std::get<std::string>(*d4) == "24:00");
  const uint8_t ttq_2415[1] = {0x61};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc15, ByteView(ttq_2415, 1)).has_value());
  const uint8_t ttq_big[1] = {0x7f};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc15, ByteView(ttq_big, 1)).has_value());

  // Null (variant and token) encodes to the wire replacement 0x00.
  Sequence enc_null = ebus::encode(DataType::time_trunc15, ebus::nullValue());
  REQUIRE(enc_null.size() == 1);
  REQUIRE(enc_null[0] == 0x00);
  Sequence enc_token =
      ebus::encode(DataType::time_trunc15, std::string("null"));
  REQUIRE(enc_token.size() == 1);
  REQUIRE(enc_token[0] == 0x00);

  // 24:00 encodes within the step range.
  Sequence enc2 = ebus::encode(DataType::time_trunc15, std::string("24:00"));
  REQUIRE(enc2.size() == 1);
  REQUIRE(enc2[0] == 0x60);

  // isValid: zero (null result) and 0x1d are valid, 0x7f is not.
  REQUIRE(ebus::isValid(DataType::time_trunc15, ByteView(ttq_zero, 1)));
  REQUIRE(ebus::isValid(DataType::time_trunc15, ByteView(ttq, 1)));
  REQUIRE_FALSE(ebus::isValid(DataType::time_trunc15, ByteView(ttq_big, 1)));
}

TEST_CASE("Datatypes: time_trunc30", "[models][datatypes]") {
  // 1 byte, low 6 bits, 30-minute steps, wire replacement 0x00.
  const uint8_t ttm[1] = {0x0f};  // 15 steps -> 07:30
  auto d1 = ebus::decode(DataType::time_trunc30, ByteView(ttm, 1));
  REQUIRE(d1);
  REQUIRE(std::get<std::string>(*d1) == "07:30");

  // Write rounds to the nearest step: 07:10 -> 07:00 (14 steps).
  Sequence enc1 = ebus::encode(DataType::time_trunc30, std::string("07:10"));
  REQUIRE(enc1.size() == 1);
  REQUIRE(enc1[0] == 0x0e);

  // Wire zero is the replacement -> null; bits above 6 are masked off.
  const uint8_t ttm_zero[1] = {0x00};
  auto d2 = ebus::decode(DataType::time_trunc30, ByteView(ttm_zero, 1));
  REQUIRE(d2);
  REQUIRE(ebus::isNull(*d2));
  const uint8_t ttm_bits[1] = {0x40};  // masked -> 00:00
  auto d3 = ebus::decode(DataType::time_trunc30, ByteView(ttm_bits, 1));
  REQUIRE(d3);
  REQUIRE(std::get<std::string>(*d3) == "00:00");

  // 24:00 fits the step range; 24:30 and 31:30 do not decode.
  const uint8_t ttm_2400[1] = {0x30};
  auto d4 = ebus::decode(DataType::time_trunc30, ByteView(ttm_2400, 1));
  REQUIRE(d4);
  REQUIRE(std::get<std::string>(*d4) == "24:00");
  const uint8_t ttm_2430[1] = {0x31};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc30, ByteView(ttm_2430, 1)).has_value());
  const uint8_t ttm_big[1] = {0x3f};
  REQUIRE_FALSE(
      ebus::decode(DataType::time_trunc30, ByteView(ttm_big, 1)).has_value());

  // Null (variant and token) encodes to the wire replacement 0x00.
  Sequence enc_null = ebus::encode(DataType::time_trunc30, ebus::nullValue());
  REQUIRE(enc_null.size() == 1);
  REQUIRE(enc_null[0] == 0x00);
  Sequence enc_token =
      ebus::encode(DataType::time_trunc30, std::string("null"));
  REQUIRE(enc_token.size() == 1);
  REQUIRE(enc_token[0] == 0x00);

  // 24:00 encodes within the step range.
  Sequence enc2 = ebus::encode(DataType::time_trunc30, std::string("24:00"));
  REQUIRE(enc2.size() == 1);
  REQUIRE(enc2[0] == 0x30);

  // isValid: zero (null result) and 0x0f are valid, 0x3f is not.
  REQUIRE(ebus::isValid(DataType::time_trunc30, ByteView(ttm_zero, 1)));
  REQUIRE(ebus::isValid(DataType::time_trunc30, ByteView(ttm, 1)));
  REQUIRE_FALSE(ebus::isValid(DataType::time_trunc30, ByteView(ttm_big, 1)));
}

TEST_CASE("Datatypes: date/time type names and sizes", "[models][datatypes]") {
  REQUIRE(ebus::stringToDataType("DATE_BCD3") == DataType::date_bcd3);
  REQUIRE(ebus::stringToDataType("DATE_BCD4") == DataType::date_bcd4);
  REQUIRE(ebus::stringToDataType("DATE_BCD4Z") == DataType::date_bcd4z);
  REQUIRE(ebus::stringToDataType("DATE_BIN3") == DataType::date_bin3);
  REQUIRE(ebus::stringToDataType("DATE_BIN4") == DataType::date_bin4);
  REQUIRE(ebus::stringToDataType("DATE_DAY2") == DataType::date_day2);
  REQUIRE(ebus::stringToDataType("DATETIME_MIN4") == DataType::datetime_min4);
  REQUIRE(ebus::stringToDataType("PIN2") == DataType::pin2);
  REQUIRE(ebus::stringToDataType("TIME_BCD2R") == DataType::time_bcd2r);
  REQUIRE(ebus::stringToDataType("TIME_BCD3R") == DataType::time_bcd3r);
  REQUIRE(ebus::stringToDataType("TIME_BIN2") == DataType::time_bin2);
  REQUIRE(ebus::stringToDataType("TIME_BIN2R") == DataType::time_bin2r);
  REQUIRE(ebus::stringToDataType("TIME_BIN3") == DataType::time_bin3);
  REQUIRE(ebus::stringToDataType("TIME_BIN3R") == DataType::time_bin3r);
  REQUIRE(ebus::stringToDataType("TIME_MIN2") == DataType::time_min2);
  REQUIRE(ebus::stringToDataType("TIME_TRUNC10") == DataType::time_trunc10);
  REQUIRE(ebus::stringToDataType("TIME_TRUNC15") == DataType::time_trunc15);
  REQUIRE(ebus::stringToDataType("TIME_TRUNC30") == DataType::time_trunc30);
  REQUIRE(ebus::stringToDataType("DATE_BCD5") == DataType::error);
  REQUIRE(ebus::stringToDataType("TIME") == DataType::error);

  REQUIRE(std::string(ebus::dataTypeToString(DataType::date_bcd4)) ==
          "DATE_BCD4");
  REQUIRE(std::string(ebus::dataTypeToString(DataType::datetime_min4)) ==
          "DATETIME_MIN4");
  REQUIRE(std::string(ebus::dataTypeToString(DataType::time_trunc10)) ==
          "TIME_TRUNC10");

  REQUIRE(ebus::sizeOfDataType(DataType::date_bcd3) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::date_bcd4) == 4);
  REQUIRE(ebus::sizeOfDataType(DataType::date_bcd4z) == 4);
  REQUIRE(ebus::sizeOfDataType(DataType::date_bin3) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::date_bin4) == 4);
  REQUIRE(ebus::sizeOfDataType(DataType::date_day2) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::datetime_min4) == 4);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bcd2r) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bcd3r) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bin2) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bin2r) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bin3) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::time_bin3r) == 3);
  REQUIRE(ebus::sizeOfDataType(DataType::time_min2) == 2);
  REQUIRE(ebus::sizeOfDataType(DataType::time_trunc10) == 1);
  REQUIRE(ebus::sizeOfDataType(DataType::time_trunc15) == 1);
  REQUIRE(ebus::sizeOfDataType(DataType::time_trunc30) == 1);
  REQUIRE(ebus::sizeOfDataType(DataType::pin2) == 2);

  // String-semantic types are not numerics (HA text path, mapper checks).
  REQUIRE(ebus::isNumeric(DataType::date_bcd3) == false);
  REQUIRE(ebus::isNumeric(DataType::date_bcd4) == false);
  REQUIRE(ebus::isNumeric(DataType::date_bcd4z) == false);
  REQUIRE(ebus::isNumeric(DataType::date_bin3) == false);
  REQUIRE(ebus::isNumeric(DataType::date_bin4) == false);
  REQUIRE(ebus::isNumeric(DataType::date_day2) == false);
  REQUIRE(ebus::isNumeric(DataType::datetime_min4) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bcd2r) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bcd3r) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bin2) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bin2r) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bin3) == false);
  REQUIRE(ebus::isNumeric(DataType::time_bin3r) == false);
  REQUIRE(ebus::isNumeric(DataType::time_min2) == false);
  REQUIRE(ebus::isNumeric(DataType::time_trunc10) == false);
  REQUIRE(ebus::isNumeric(DataType::time_trunc15) == false);
  REQUIRE(ebus::isNumeric(DataType::time_trunc30) == false);
  REQUIRE(ebus::isNumeric(DataType::pin2) == false);
}
