/*
 * Copyright (C) 2017-2025 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "Datatypes.hpp"

using namespace ebus;

void test_bcd() {
  for (uint8_t v = 0; v <= 99; ++v) {
    std::vector<uint8_t> bytes = bcd_2_byte(v);
    uint8_t decoded = byte_2_bcd(bytes);
    assert(decoded == v);
  }
  // Invalid BCD
  std::vector<uint8_t> invalid = {0xAA};
  assert(byte_2_bcd(invalid) == 0xFF);
}

void test_uint8() {
  for (uint16_t v = 0; v <= 0xFF; ++v) {
    uint8_t value = static_cast<uint8_t>(v);
    std::vector<uint8_t> bytes = uint8_2_byte(value);
    uint8_t decoded = byte_2_uint8(bytes);
    assert(decoded == value);
    assert(bytes.size() == 1 && bytes[0] == value);
  }
}

void test_int8() {
  for (int16_t v = -128; v <= 127; ++v) {
    int8_t value = static_cast<int8_t>(v);
    std::vector<uint8_t> bytes = int8_2_byte(value);
    int8_t decoded = byte_2_int8(bytes);
    assert(decoded == value);
  }
}

void test_uint16() {
  for (uint32_t v = 0; v <= 0xFFFF; v += 257) {  // step to keep test fast
    uint16_t value = static_cast<uint16_t>(v);
    std::vector<uint8_t> bytes = uint16_2_byte(value);
    uint16_t decoded = byte_2_uint16(bytes);
    assert(decoded == value);
  }
}

void test_int16() {
  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    std::vector<uint8_t> bytes = int16_2_byte(value);
    int16_t decoded = byte_2_int16(bytes);
    assert(decoded == value);
  }
}

void test_uint32() {
  std::vector<uint32_t> test_values = {0, 1, 0xFFFFFFFF, 0x12345678,
                                       0x80000000};
  for (uint32_t value : test_values) {
    std::vector<uint8_t> bytes = uint32_2_byte(value);
    uint32_t decoded = byte_2_uint32(bytes);
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
    std::vector<uint8_t> bytes = int32_2_byte(value);
    int32_t decoded = byte_2_int32(bytes);
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
    std::vector<uint8_t> bytes = float_2_byte(value);
    double_t decoded = byte_2_float(bytes);
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

void test_data1b() {
  for (int16_t v = -128; v <= 127; v += 17) {
    double_t value = static_cast<double_t>(v);
    std::vector<uint8_t> bytes = data1b_2_byte(value);
    double_t decoded = byte_2_data1b(bytes);
    assert(std::fabs(decoded - value) < 1e-5);
  }
}

void test_data1c() {
  for (uint16_t v = 0; v <= 255; v += 17) {
    double_t value = static_cast<double_t>(v) / 2.0;
    std::vector<uint8_t> bytes = data1c_2_byte(value);
    double_t decoded = byte_2_data1c(bytes);
    assert(std::fabs(decoded - value) < 1e-5);
  }
}

void test_data2b() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    std::vector<uint8_t> bytes = data2b_2_byte(value);
    double_t decoded = byte_2_data2b(bytes);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_data2c() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    std::vector<uint8_t> bytes = data2c_2_byte(value);
    double_t decoded = byte_2_data2c(bytes);
    assert(std::fabs(decoded - value) < 1e-3);
  }
}

void test_string() {
  std::vector<std::string> test_strings = {"", "A", "Hello", "ebus123",
                                           std::string(8, 'Z')};
  for (const auto& str : test_strings) {
    std::vector<uint8_t> bytes = string_2_byte(str);
    std::string decoded = byte_2_string(bytes);
    assert(decoded == str);
  }
}

int main() {
  test_bcd();
  test_uint8();
  test_int8();
  test_uint16();
  test_int16();
  test_uint32();
  test_int32();
  test_float();
  test_data1b();
  test_data1c();
  test_data2b();
  test_data2c();
  test_string();

  std::cout << "All datatype conversion tests passed!" << std::endl;
  return 0;
}