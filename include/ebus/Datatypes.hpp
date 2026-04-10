/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

enum class Endian { Little, Big };

enum class DataType {
  ERROR = -1,
  BCD,
  UINT8,
  INT8,
  DATA1B,
  DATA1C,
  UINT16,
  UINT16R,
  INT16,
  INT16R,
  DATA2B,
  DATA2BR,
  DATA2C,
  DATA2CR,
  UINT32,
  UINT32R,
  INT32,
  INT32R,
  FLOAT,
  FLOATR,
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
double_t round_digits(const double_t& value, const uint8_t& digits);

uint8_t byte_2_bcd(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> bcd_2_byte(const uint8_t& value);

uint8_t byte_2_uint8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> uint8_2_byte(const uint8_t& value);

int8_t byte_2_int8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int8_2_byte(const int8_t& value);

double_t byte_2_data1b(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1b_2_byte(const double_t& value);

double_t byte_2_data1c(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1c_2_byte(const double_t& value);

uint16_t byte_2_uint16(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> uint16_2_byte(const uint16_t& value, Endian e);

int16_t byte_2_int16(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> int16_2_byte(const int16_t& value, Endian e);

double_t byte_2_data2b(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> data2b_2_byte(const double_t& value, Endian e);

double_t byte_2_data2c(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> data2c_2_byte(const double_t& value, Endian e);

uint32_t byte_2_uint32(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> uint32_2_byte(const uint32_t& value, Endian e);

int32_t byte_2_int32(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> int32_2_byte(const int32_t& value, Endian e);

double_t byte_2_float(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> float_2_byte(const double_t& value, Endian e);

std::string byte_2_char(const std::vector<uint8_t>& vec);
std::vector<uint8_t> char_2_byte(const std::string& str);

std::string byte_2_hex(const std::vector<uint8_t>& vec);
std::vector<uint8_t> hex_2_byte(const std::string& str);

}  // namespace ebus