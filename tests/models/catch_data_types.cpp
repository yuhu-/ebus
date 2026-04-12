/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cctype>
#include <cmath>
#include <ebus/data_types.hpp>
#include <limits>
#include <vector>

using namespace ebus;

TEST_CASE("Datatypes: BCD", "[models][datatypes]") {
  for (uint8_t v = 0; v <= 99; ++v) {
    std::vector<uint8_t> bytes = bcdToByte(v);
    uint8_t decoded = byteToBcd(bytes);
    REQUIRE(decoded == v);
  }
  std::vector<uint8_t> invalid = {0xaa};
  REQUIRE(byteToBcd(invalid) == 0xff);
}

TEST_CASE("Datatypes: uint8/int8/data1b/data1c", "[models][datatypes]") {
  for (uint16_t v = 0; v <= 0xff; ++v) {
    uint8_t value = static_cast<uint8_t>(v);
    std::vector<uint8_t> bytes = uint8ToByte(value);
    uint8_t decoded = byteToUint8(bytes);
    REQUIRE(decoded == value);
    REQUIRE(bytes.size() == 1);
    REQUIRE(bytes[0] == value);
  }

  for (int16_t v = -128; v <= 127; ++v) {
    int8_t value = static_cast<int8_t>(v);
    std::vector<uint8_t> bytes = int8ToByte(value);
    int8_t decoded = byteToInt8(bytes);
    REQUIRE(decoded == value);
  }

  for (int16_t v = -128; v <= 127; v += 17) {
    double_t value = static_cast<double_t>(v);
    std::vector<uint8_t> bytes = data1bToByte(value);
    double_t decoded = byteToData1b(bytes);
    REQUIRE(std::fabs(decoded - value) < 1e-5);
  }

  for (uint16_t v = 0; v <= 255; v += 17) {
    double_t value = static_cast<double_t>(v) / 2.0;
    std::vector<uint8_t> bytes = data1cToByte(value);
    double_t decoded = byteToData1c(bytes);
    REQUIRE(std::fabs(decoded - value) < 1e-5);
  }
}

TEST_CASE("Datatypes: 16-bit and data2", "[models][datatypes]") {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {
    uint16_t value = static_cast<uint16_t>(v);
    std::vector<uint8_t> bytes = uint16ToByte(value, Endian::little);
    uint16_t decoded = byteToUint16(bytes, Endian::little);
    REQUIRE(decoded == value);
  }

  for (uint32_t v = 0; v <= 0xffff; v += 257) {
    uint16_t value = static_cast<uint16_t>(v);
    std::vector<uint8_t> bytes = uint16ToByte(value, Endian::big);
    uint16_t decoded = byteToUint16(bytes, Endian::big);
    REQUIRE(decoded == value);
  }

  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    std::vector<uint8_t> bytes = int16ToByte(value, Endian::little);
    int16_t decoded = byteToInt16(bytes, Endian::little);
    REQUIRE(decoded == value);
  }

  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    std::vector<uint8_t> bytes = int16ToByte(value, Endian::big);
    int16_t decoded = byteToInt16(bytes, Endian::big);
    REQUIRE(decoded == value);
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    std::vector<uint8_t> bytes = data2bToByte(value, Endian::little);
    double_t decoded = byteToData2b(bytes, Endian::little);
    REQUIRE(std::fabs(decoded - value) < 1e-3);
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    std::vector<uint8_t> bytes = data2bToByte(value, Endian::big);
    double_t decoded = byteToData2b(bytes, Endian::big);
    REQUIRE(std::fabs(decoded - value) < 1e-3);
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    std::vector<uint8_t> bytes = data2cToByte(value, Endian::little);
    double_t decoded = byteToData2c(bytes, Endian::little);
    REQUIRE(std::fabs(decoded - value) < 1e-3);
  }

  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    std::vector<uint8_t> bytes = data2cToByte(value, Endian::big);
    double_t decoded = byteToData2c(bytes, Endian::big);
    REQUIRE(std::fabs(decoded - value) < 1e-3);
  }
}

TEST_CASE("Datatypes: 32-bit ints and floats", "[models][datatypes]") {
  std::vector<uint32_t> u32_vals = {0, 1, 0xffffffff, 0x12345678, 0x80000000};
  for (uint32_t value : u32_vals) {
    std::vector<uint8_t> bytes = uint32ToByte(value, Endian::little);
    uint32_t decoded = byteToUint32(bytes, Endian::little);
    REQUIRE(decoded == value);
  }

  for (uint32_t value : u32_vals) {
    std::vector<uint8_t> bytes = uint32ToByte(value, Endian::big);
    uint32_t decoded = byteToUint32(bytes, Endian::big);
    REQUIRE(decoded == value);
  }

  std::vector<int32_t> i32_vals = {0,
                                   1,
                                   -1,
                                   std::numeric_limits<int32_t>::max(),
                                   std::numeric_limits<int32_t>::min(),
                                   0x12345678};
  for (int32_t value : i32_vals) {
    std::vector<uint8_t> bytes = int32ToByte(value, Endian::little);
    int32_t decoded = byteToInt32(bytes, Endian::little);
    REQUIRE(decoded == value);
  }

  for (int32_t value : i32_vals) {
    std::vector<uint8_t> bytes = int32ToByte(value, Endian::big);
    int32_t decoded = byteToInt32(bytes, Endian::big);
    REQUIRE(decoded == value);
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
    std::vector<uint8_t> bytes = floatToByte(value, Endian::little);
    double_t decoded = byteToFloat(bytes, Endian::little);
    if (std::isnan(value)) {
      REQUIRE(std::isnan(decoded));
    } else if (std::isinf(value)) {
      REQUIRE(std::isinf(decoded));
      REQUIRE(std::signbit(value) == std::signbit(decoded));
    } else {
      REQUIRE(std::fabs(decoded - value) < 1e-5);
    }
  }

  for (float value : float_vals) {
    std::vector<uint8_t> bytes = floatToByte(value, Endian::big);
    double_t decoded = byteToFloat(bytes, Endian::big);
    if (std::isnan(value)) {
      REQUIRE(std::isnan(decoded));
    } else if (std::isinf(value)) {
      REQUIRE(std::isinf(decoded));
      REQUIRE(std::signbit(value) == std::signbit(decoded));
    } else {
      REQUIRE(std::fabs(decoded - value) < 1e-5);
    }
  }
}

TEST_CASE("Datatypes: char and hex", "[models][datatypes]") {
  std::vector<std::string> test_strings = {"", "A", "Hello", "ebus123",
                                           std::string(8, 'Z')};
  for (const std::string& str : test_strings) {
    std::vector<uint8_t> bytes = charToByte(str);
    std::string decoded = byteToChar(bytes);
    REQUIRE(decoded == str);
  }

  std::vector<std::string> hex_strings = {"",       "00",     "FF",      "1234",
                                          "abcdef", "ABCDEF", "deadbeef"};
  for (const std::string& str : hex_strings) {
    std::vector<uint8_t> bytes = hexToByte(str);
    std::string decoded = byteToHex(bytes);
    std::string str_lower = str;
    std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(),
                   ::tolower);
    REQUIRE(decoded == str_lower);
  }
}