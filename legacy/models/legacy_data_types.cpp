/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <ebus/data_types.hpp>
#include <iostream>
#include <limits>
#include <vector>

using namespace ebus;

void test_bcd() {
  for (uint8_t v = 0; v <= 99; ++v) {
    std::vector<uint8_t> bytes = bcdToByte(v);
    uint8_t decoded = byteToBcd(bytes);
    assert(decoded == v);
  }
  // Invalid BCD
  std::vector<uint8_t> invalid = {0xaa};
  assert(byteToBcd(invalid) == 0xff);
}

void test_uint8() {
  for (uint16_t v = 0; v <= 0xff; ++v) {
    uint8_t value = static_cast<uint8_t>(v);
    std::vector<uint8_t> bytes = uint8ToByte(value);
    uint8_t decoded = byteToUint8(bytes);
    assert(decoded == value);
    assert(bytes.size() == 1 && bytes[0] == value);
  }
}

void test_int8() {
  for (int16_t v = -128; v <= 127; ++v) {
    int8_t value = static_cast<int8_t>(v);
    std::vector<uint8_t> bytes = int8ToByte(value);
    int8_t decoded = byteToInt8(bytes);
    assert(decoded == value);
  }
}

void test_data1b() {
  for (int16_t v = -128; v <= 127; v += 17) {
    double_t value = static_cast<double_t>(v);
    std::vector<uint8_t> bytes = data1bToByte(value);
    double_t decoded = byteToData1b(bytes);
    assert(std::fabs(decoded - value) < 1e-5);
  }
}

void test_data1c() {
  for (uint16_t v = 0; v <= 255; v += 17) {
    double_t value = static_cast<double_t>(v) / 2.0;
    std::vector<uint8_t> bytes = data1cToByte(value);
    double_t decoded = byteToData1c(bytes);
    assert(std::fabs(decoded - value) < 1e-5);
  }
}

void test_uint16() {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {  // step to keep test fast
    uint16_t value = static_cast<uint16_t>(v);
    std::vector<uint8_t> bytes = uint16ToByte(value, Endian::little);
    uint16_t decoded = byteToUint16(bytes, Endian::little);
    assert(decoded == value);
  }
}

void test_uint16r() {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {  // step to keep test fast
    uint16_t value = static_cast<uint16_t>(v);
    std::vector<uint8_t> bytes = uint16ToByte(value, Endian::big);
    uint16_t decoded = byteToUint16(bytes, Endian::big);
    assert(decoded == value);
  }
}

void test_int16() {
  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    std::vector<uint8_t> bytes = int16ToByte(value, Endian::little);
    int16_t decoded = byteToInt16(bytes, Endian::little);
    assert(decoded == value);
  }
}

void test_int16r() {
  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    std::vector<uint8_t> bytes = int16ToByte(value, Endian::big);
    int16_t decoded = byteToInt16(bytes, Endian::big);
    assert(decoded == value);
  }
}

void test_data2b() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    std::vector<uint8_t> bytes = data2bToByte(value, Endian::little);
    double_t decoded = byteToData2b(bytes, Endian::little);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_data2br() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    std::vector<uint8_t> bytes = data2bToByte(value, Endian::big);
    double_t decoded = byteToData2b(bytes, Endian::big);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_data2c() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    std::vector<uint8_t> bytes = data2cToByte(value, Endian::little);
    double_t decoded = byteToData2c(bytes, Endian::little);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_data2cr() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    std::vector<uint8_t> bytes = data2cToByte(value, Endian::big);
    double_t decoded = byteToData2c(bytes, Endian::big);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_uint32() {
  std::vector<uint32_t> test_values = {0, 1, 0xffffffff, 0x12345678,
                                       0x80000000};
  for (uint32_t value : test_values) {
    std::vector<uint8_t> bytes = uint32ToByte(value, Endian::little);
    uint32_t decoded = byteToUint32(bytes, Endian::little);
    assert(decoded == value);
  }
}

void test_uint32r() {
  std::vector<uint32_t> test_values = {0, 1, 0xffffffff, 0x12345678,
                                       0x80000000};
  for (uint32_t value : test_values) {
    std::vector<uint8_t> bytes = uint32ToByte(value, Endian::big);
    uint32_t decoded = byteToUint32(bytes, Endian::big);
    assert(decoded == value);
  }
}

void test_int32() {
  std::vector<int32_t> test_values = {0,
                                      1,
                                      -1,
                                      std::numeric_limits<int32_t>::max(),
                                      std::numeric_limits<int32_t>::min(),
                                      0x12345678};
  for (int32_t value : test_values) {
    std::vector<uint8_t> bytes = int32ToByte(value, Endian::little);
    int32_t decoded = byteToInt32(bytes, Endian::little);
    assert(decoded == value);
  }
}

void test_int32r() {
  std::vector<int32_t> test_values = {0,
                                      1,
                                      -1,
                                      std::numeric_limits<int32_t>::max(),
                                      std::numeric_limits<int32_t>::min(),
                                      0x12345678};
  for (int32_t value : test_values) {
    std::vector<uint8_t> bytes = int32ToByte(value, Endian::big);
    int32_t decoded = byteToInt32(bytes, Endian::big);
    assert(decoded == value);
  }
}

void test_float() {
  std::vector<float> test_values = {0.0f,
                                    1.0f,
                                    -1.0f,
                                    123.456f,
                                    -789.123f,
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity(),
                                    std::nanf("")};
  for (float value : test_values) {
    std::vector<uint8_t> bytes = floatToByte(value, Endian::little);
    double_t decoded = byteToFloat(bytes, Endian::little);
    if (std::isnan(value)) {
      assert(std::isnan(decoded));
    } else if (std::isinf(value)) {
      assert(std::isinf(decoded) &&
             (std::signbit(value) == std::signbit(decoded)));
    } else {
      assert(std::fabs(decoded - value) < 1e-5);
    }
  }
}

void test_floatr() {
  std::vector<float> test_values = {0.0f,
                                    1.0f,
                                    -1.0f,
                                    123.456f,
                                    -789.123f,
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity(),
                                    std::nanf("")};
  for (float value : test_values) {
    std::vector<uint8_t> bytes = floatToByte(value, Endian::big);
    double_t decoded = byteToFloat(bytes, Endian::big);
    if (std::isnan(value)) {
      assert(std::isnan(decoded));
    } else if (std::isinf(value)) {
      assert(std::isinf(decoded) &&
             (std::signbit(value) == std::signbit(decoded)));
    } else {
      assert(std::fabs(decoded - value) < 1e-5);
    }
  }
}

void test_char() {
  std::vector<std::string> test_strings = {"", "A", "Hello", "ebus123",
                                           std::string(8, 'Z')};
  for (const std::string& str : test_strings) {
    std::vector<uint8_t> bytes = charToByte(str);
    std::string decoded = byteToChar(bytes);
    assert(decoded == str);
  }
}

void test_hex() {
  std::vector<std::string> test_strings = {
      "", "00", "FF", "1234", "abcdef", "ABCDEF", "deadbeef"};
  for (const std::string& str : test_strings) {
    std::vector<uint8_t> bytes = hexToByte(str);
    std::string decoded = byteToHex(bytes);
    std::string str_lower = str;
    std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(),
                   ::tolower);
    assert(decoded == str_lower);
  }
}

int main() {
  test_bcd();
  test_uint8();
  test_int8();
  test_data1b();
  test_data1c();
  test_uint16();
  test_uint16r();
  test_int16();
  test_int16r();
  test_data2b();
  test_data2br();
  test_data2c();
  test_data2cr();
  test_uint32();
  test_uint32r();
  test_int32();
  test_int32r();
  test_float();
  test_floatr();
  test_char();
  test_hex();

  std::cout << "All datatype conversion tests passed!" << std::endl;
  return 0;
}