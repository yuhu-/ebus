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

// This file offers various functions for decoding/encoding in accordance with
// the ebus data types and beyond.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

// available data types
enum class DataType {
  ERROR = -1,
  BCD,
  UINT8,
  INT8,
  UINT16,
  INT16,
  UINT32,
  INT32,
  DATA1B,
  DATA1C,
  DATA2B,
  DATA2C,
  FLOAT,
  CHAR1,
  CHAR2,
  CHAR3,
  CHAR4,
  CHAR5,
  CHAR6,
  CHAR7,
  CHAR8,
  HEX1,
  HEX2,
  HEX3,
  HEX4,
  HEX5,
  HEX6,
  HEX7,
  HEX8
};

const char* datatype_2_string(const DataType& datatype);
DataType string_2_datatype(const char* str);

size_t sizeof_datatype(const DataType& datatype);
bool typeof_datatype(const DataType& datatype);

// rounding helper
double_t round_digits(const double_t& value, const uint8_t& digits);

// Generic integer conversion helpers
template <typename T>
T byte_2_int(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != sizeof(T)) return 0;
  T value = 0;
  for (size_t i = 0; i < sizeof(T); ++i)
    value |= static_cast<T>(bytes[i]) << (8 * i);
  return value;
}

template <typename T>
std::vector<uint8_t> int_2_byte(const T& value) {
  std::vector<uint8_t> bytes(sizeof(T));
  for (size_t i = 0; i < sizeof(T); ++i)
    bytes[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
  return bytes;
}

// BCD
uint8_t byte_2_bcd(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> bcd_2_byte(const uint8_t& value);

// UINT8
uint8_t byte_2_uint8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> uint8_2_byte(const uint8_t& value);

// INT8
int8_t byte_2_int8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int8_2_byte(const int8_t& value);

// UINT16
uint16_t byte_2_uint16(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> uint16_2_byte(const uint16_t& value);

// INT16
int16_t byte_2_int16(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int16_2_byte(const int16_t& value);

// UINT32
uint32_t byte_2_uint32(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> uint32_2_byte(const uint32_t& value);

// INT32
int32_t byte_2_int32(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int32_2_byte(const int32_t& value);

// DATA1B
double_t byte_2_data1b(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1b_2_byte(const double_t& value);

// DATA1C
double_t byte_2_data1c(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1c_2_byte(const double_t& value);

// DATA2B
double_t byte_2_data2b(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data2b_2_byte(const double_t& value);

// DATA2C
double_t byte_2_data2c(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data2c_2_byte(const double_t& value);

// FLOAT (IEEE 754 single precision)
double_t byte_2_float(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> float_2_byte(const double_t& value);

// CHAR
std::string byte_2_char(const std::vector<uint8_t>& vec);
std::vector<uint8_t> char_2_byte(const std::string& str);

// HEX
std::string byte_2_hex(const std::vector<uint8_t>& vec);
std::vector<uint8_t> hex_2_byte(const std::string& str);

}  // namespace ebus
