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

#include "Datatypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>
#include <utility>

#include "Common.hpp"

std::map<ebus::DataType, const char*> DatatypeName = {
    {ebus::DataType::ERROR, "ERROR"},   {ebus::DataType::BCD, "BCD"},
    {ebus::DataType::UINT8, "UINT8"},   {ebus::DataType::INT8, "INT8"},
    {ebus::DataType::UINT16, "UINT16"}, {ebus::DataType::INT16, "INT16"},
    {ebus::DataType::UINT32, "UINT32"}, {ebus::DataType::INT32, "INT32"},
    {ebus::DataType::DATA1B, "DATA1B"}, {ebus::DataType::DATA1C, "DATA1C"},
    {ebus::DataType::DATA2B, "DATA2B"}, {ebus::DataType::DATA2C, "DATA2C"},
    {ebus::DataType::FLOAT, "FLOAT"},   {ebus::DataType::CHAR1, "CHAR1"},
    {ebus::DataType::CHAR2, "CHAR2"},   {ebus::DataType::CHAR3, "CHAR3"},
    {ebus::DataType::CHAR4, "CHAR4"},   {ebus::DataType::CHAR5, "CHAR5"},
    {ebus::DataType::CHAR6, "CHAR6"},   {ebus::DataType::CHAR7, "CHAR7"},
    {ebus::DataType::CHAR8, "CHAR8"},   {ebus::DataType::HEX1, "HEX1"},
    {ebus::DataType::HEX2, "HEX2"},     {ebus::DataType::HEX3, "HEX3"},
    {ebus::DataType::HEX4, "HEX4"},     {ebus::DataType::HEX5, "HEX5"},
    {ebus::DataType::HEX6, "HEX6"},     {ebus::DataType::HEX7, "HEX7"},
    {ebus::DataType::HEX8, "HEX8"}};

const char* ebus::datatype_2_string(const ebus::DataType& datatype) {
  return DatatypeName[datatype];
}

ebus::DataType ebus::string_2_datatype(const char* str) {
  DataType datatype = DataType::ERROR;

  const std::map<ebus::DataType, const char*>::const_iterator it =
      std::find_if(DatatypeName.begin(), DatatypeName.end(),
                   [&str](const std::pair<DataType, const char*>& item) {
                     return strcmp(str, item.second) == 0;
                   });

  if (it != DatatypeName.end()) datatype = it->first;

  return datatype;
}

size_t ebus::sizeof_datatype(const ebus::DataType& datatype) {
  size_t length = 0;

  switch (datatype) {
    case ebus::DataType::BCD:
    case ebus::DataType::UINT8:
    case ebus::DataType::INT8:
    case ebus::DataType::DATA1B:
    case ebus::DataType::DATA1C:
    case ebus::DataType::CHAR1:
    case ebus::DataType::HEX1:
      length = 1;
      break;
    case ebus::DataType::UINT16:
    case ebus::DataType::INT16:
    case ebus::DataType::DATA2B:
    case ebus::DataType::DATA2C:
    case ebus::DataType::CHAR2:
    case ebus::DataType::HEX2:
      length = 2;
      break;
    case ebus::DataType::CHAR3:
    case ebus::DataType::HEX3:
      length = 3;
      break;
    case ebus::DataType::UINT32:
    case ebus::DataType::INT32:
    case ebus::DataType::FLOAT:
    case ebus::DataType::CHAR4:
    case ebus::DataType::HEX4:
      length = 4;
      break;
    case ebus::DataType::CHAR5:
    case ebus::DataType::HEX5:
      length = 5;
      break;
    case ebus::DataType::CHAR6:
    case ebus::DataType::HEX6:
      length = 6;
      break;
    case ebus::DataType::CHAR7:
    case ebus::DataType::HEX7:
      length = 7;
      break;
    case ebus::DataType::CHAR8:
    case ebus::DataType::HEX8:
      length = 8;
      break;
    default:
      break;
  }
  return length;
}

bool ebus::typeof_datatype(const DataType& datatype) {
  bool numeric = true;

  switch (datatype) {
    case ebus::DataType::CHAR1:
    case ebus::DataType::CHAR2:
    case ebus::DataType::CHAR3:
    case ebus::DataType::CHAR4:
    case ebus::DataType::CHAR5:
    case ebus::DataType::CHAR6:
    case ebus::DataType::CHAR7:
    case ebus::DataType::CHAR8:
    case ebus::DataType::HEX1:
    case ebus::DataType::HEX2:
    case ebus::DataType::HEX3:
    case ebus::DataType::HEX4:
    case ebus::DataType::HEX5:
    case ebus::DataType::HEX6:
    case ebus::DataType::HEX7:
    case ebus::DataType::HEX8:
      numeric = false;
      break;
    default:
      break;
  }

  return numeric;
}

double_t ebus::round_digits(const double_t& value, const uint8_t& digits) {
  double_t fractpart, intpart;
  fractpart = std::modf(value, &intpart);

  double_t decimals = std::pow(10, digits);

  return static_cast<double_t>(intpart) +
         std::round(fractpart * decimals) / decimals;
}

// BCD
uint8_t ebus::byte_2_bcd(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0xff;
  uint8_t value = bytes[0];
  if ((value & 0x0f) > 9 || ((value >> 4) & 0x0f) > 9) return 0xff;
  return (value >> 4) * 10 + (value & 0x0f);
}

std::vector<uint8_t> ebus::bcd_2_byte(const uint8_t& value) {
  if (value > 99) return {0xff};
  uint8_t bcd = ((value / 10) << 4) | (value % 10);
  return {bcd};
}

// UINT8
uint8_t ebus::byte_2_uint8(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0;
  return bytes[0];
}

std::vector<uint8_t> ebus::uint8_2_byte(const uint8_t& value) {
  return {value};
}

// INT8
int8_t ebus::byte_2_int8(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0;
  return static_cast<int8_t>(bytes[0]);
}

std::vector<uint8_t> ebus::int8_2_byte(const int8_t& value) {
  return {static_cast<uint8_t>(value)};
}

// UINT16
uint16_t ebus::byte_2_uint16(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 2) return 0;
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

std::vector<uint8_t> ebus::uint16_2_byte(const uint16_t& value) {
  return {static_cast<uint8_t>(value & 0xff),
          static_cast<uint8_t>((value >> 8) & 0xff)};
}

// INT16
int16_t ebus::byte_2_int16(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 2) return 0;
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) |
                              (static_cast<uint16_t>(bytes[1]) << 8));
}

std::vector<uint8_t> ebus::int16_2_byte(const int16_t& value) {
  uint16_t uval = static_cast<uint16_t>(value);
  return {static_cast<uint8_t>(uval & 0xff),
          static_cast<uint8_t>((uval >> 8) & 0xff)};
}

// UINT32
uint32_t ebus::byte_2_uint32(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 4) return 0;
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

std::vector<uint8_t> ebus::uint32_2_byte(const uint32_t& value) {
  return {static_cast<uint8_t>(value & 0xff),
          static_cast<uint8_t>((value >> 8) & 0xff),
          static_cast<uint8_t>((value >> 16) & 0xff),
          static_cast<uint8_t>((value >> 24) & 0xff)};
}

// INT32
int32_t ebus::byte_2_int32(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 4) return 0;
  uint32_t uval = ebus::byte_2_uint32(bytes);
  return static_cast<int32_t>(uval);
}

std::vector<uint8_t> ebus::int32_2_byte(const int32_t& value) {
  uint32_t uval = static_cast<uint32_t>(value);
  return ebus::uint32_2_byte(uval);
}

// DATA1B
double_t ebus::byte_2_data1b(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byte_2_int8(bytes));
}

std::vector<uint8_t> ebus::data1b_2_byte(const double_t& value) {
  return ebus::int8_2_byte(static_cast<int8_t>(value));
}

// DATA1C
double_t ebus::byte_2_data1c(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byte_2_uint8(bytes)) / 2.0;
}

std::vector<uint8_t> ebus::data1c_2_byte(const double_t& value) {
  return ebus::uint8_2_byte(static_cast<uint8_t>(value * 2.0));
}

// DATA2B
double_t ebus::byte_2_data2b(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byte_2_int16(bytes)) / 256.0;
}

std::vector<uint8_t> ebus::data2b_2_byte(const double_t& value) {
  return ebus::int16_2_byte(static_cast<int16_t>(value * 256.0));
}

// DATA2C
double_t ebus::byte_2_data2c(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byte_2_int16(bytes)) / 16.0;
}

std::vector<uint8_t> ebus::data2c_2_byte(const double_t& value) {
  return ebus::int16_2_byte(static_cast<int16_t>(value * 16.0));
}

// FLOAT (IEEE 754 single precision)
double_t ebus::byte_2_float(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 4) return 0.0;
  uint32_t raw = ebus::byte_2_uint32(bytes);
  float value;
  std::memcpy(&value, &raw, sizeof(float));
  return static_cast<double_t>(value);
}

std::vector<uint8_t> ebus::float_2_byte(const double_t& value) {
  float f = static_cast<float>(value);
  uint32_t raw;
  std::memcpy(&raw, &f, sizeof(float));
  return ebus::uint32_2_byte(raw);
}

// CHAR
std::string ebus::byte_2_char(const std::vector<uint8_t>& vec) {
  return std::string(vec.begin(), vec.end());
}

std::vector<uint8_t> ebus::char_2_byte(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// HEX
std::string ebus::byte_2_hex(const std::vector<uint8_t>& vec) {
  return ebus::to_string(vec);
}

std::vector<uint8_t> ebus::hex_2_byte(const std::string& str) {
  return ebus::to_vector(str);
}
