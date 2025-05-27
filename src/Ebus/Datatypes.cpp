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
#include <cstring>
#include <map>
#include <sstream>
#include <utility>

std::map<ebus::DataType, const char *> DatatypeName = {
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
    {ebus::DataType::CHAR8, "CHAR8"},   {ebus::DataType::CHAR9, "CHAR9"},
    {ebus::DataType::CHAR10, "CHAR10"}};

const char *ebus::datatype_2_string(const ebus::DataType &datatype) {
  return DatatypeName[datatype];
}

ebus::DataType ebus::string_2_datatype(const char *str) {
  DataType datatype = DataType::ERROR;

  const std::map<ebus::DataType, const char *>::const_iterator it =
      std::find_if(DatatypeName.begin(), DatatypeName.end(),
                   [&str](const std::pair<DataType, const char *> &item) {
                     return strcmp(str, item.second) == 0;
                   });

  if (it != DatatypeName.end()) datatype = it->first;

  return datatype;
}

size_t ebus::sizeof_datatype(const ebus::DataType &datatype) {
  size_t length = 0;

  switch (datatype) {
    case ebus::DataType::BCD:
    case ebus::DataType::UINT8:
    case ebus::DataType::INT8:
    case ebus::DataType::DATA1B:
    case ebus::DataType::DATA1C:
    case ebus::DataType::CHAR1:
      length = 1;
      break;
    case ebus::DataType::UINT16:
    case ebus::DataType::INT16:
    case ebus::DataType::DATA2B:
    case ebus::DataType::DATA2C:
    case ebus::DataType::FLOAT:
    case ebus::DataType::CHAR2:
      length = 2;
      break;
    case ebus::DataType::CHAR3:
      length = 3;
      break;
    case ebus::DataType::UINT32:
    case ebus::DataType::INT32:
    case ebus::DataType::CHAR4:
      length = 4;
      break;
    case ebus::DataType::CHAR5:
      length = 5;
      break;
    case ebus::DataType::CHAR6:
      length = 6;
      break;
    case ebus::DataType::CHAR7:
      length = 7;
      break;
    case ebus::DataType::CHAR8:
      length = 8;
      break;
    case ebus::DataType::CHAR9:
      length = 9;
      break;
    case ebus::DataType::CHAR10:
      length = 10;
      break;
    default:
      break;
  }
  return length;
}

bool ebus::typeof_datatype(const DataType &datatype) {
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
    case ebus::DataType::CHAR9:
    case ebus::DataType::CHAR10:
      numeric = false;
      break;
    default:
      break;
  }

  return numeric;
}

// helper functions
uint ebus::convert_base(uint value, const uint &oldBase, const uint &newBase) {
  uint result = 0;
  for (uint i = 0; value > 0; i++) {
    result += value % oldBase * pow(newBase, i);
    value /= oldBase;
  }
  return result;
}

double_t ebus::round_digits(const double_t &value, const uint8_t &digits) {
  double_t fractpart, intpart;
  fractpart = modf(value, &intpart);

  double_t decimals = pow(10, digits);

  return static_cast<double_t>(intpart) +
         round(fractpart * decimals) / decimals;
}

// BCD
uint8_t ebus::byte_2_bcd(const std::vector<uint8_t> &bytes) {
  uint8_t value = bytes[0];
  uint8_t result = convert_base(value, 16, 10);

  if ((value & 0x0f) > 9 || ((value >> 4) & 0x0f) > 9) result = 0xff;

  return result;
}

std::vector<uint8_t> ebus::bcd_2_byte(const uint8_t &value) {
  uint8_t byte = convert_base(value, 10, 16);
  std::vector<uint8_t> result(1, byte);

  if (value > 99) result[0] = 0xff;

  return result;
}

// UINT8
uint8_t ebus::byte_2_uint8(const std::vector<uint8_t> &bytes) {
  return byte_2_int<uint8_t>(bytes);
}

std::vector<uint8_t> ebus::uint8_2_byte(const uint8_t &value) {
  return int_2_byte<uint8_t>(value);
}

// INT8
int8_t ebus::byte_2_int8(const std::vector<uint8_t> &bytes) {
  return byte_2_int<int8_t>(bytes);
}

std::vector<uint8_t> ebus::int8_2_byte(const int8_t &value) {
  return int_2_byte<int8_t>(value);
}

// UINT16
uint16_t ebus::byte_2_uint16(const std::vector<uint8_t> &bytes) {
  return byte_2_int<uint16_t>(bytes);
}

std::vector<uint8_t> ebus::uint16_2_byte(const uint16_t &value) {
  return int_2_byte<uint16_t>(value);
}

// INT16
int16_t ebus::byte_2_int16(const std::vector<uint8_t> &bytes) {
  return byte_2_int<int16_t>(bytes);
}

std::vector<uint8_t> ebus::int16_2_byte(const int16_t &value) {
  return int_2_byte<int16_t>(value);
}

// UINT32
uint32_t ebus::byte_2_uint32(const std::vector<uint8_t> &bytes) {
  return byte_2_int<uint32_t>(bytes);
}

std::vector<uint8_t> ebus::uint32_2_byte(const uint32_t &value) {
  return int_2_byte<uint32_t>(value);
}

// INT32
int32_t ebus::byte_2_int32(const std::vector<uint8_t> &bytes) {
  return byte_2_int<int32_t>(bytes);
}

std::vector<uint8_t> ebus::int32_2_byte(const int32_t &value) {
  return int_2_byte<int32_t>(value);
}

// DATA1B
double_t ebus::byte_2_data1b(const std::vector<uint8_t> &bytes) {
  return static_cast<double_t>(byte_2_int<int8_t>(bytes));
}

std::vector<uint8_t> ebus::data1b_2_byte(const double_t &value) {
  return int_2_byte(static_cast<int8_t>(value));
}

// DATA1C
double_t ebus::byte_2_data1c(const std::vector<uint8_t> &bytes) {
  return static_cast<double_t>(byte_2_int<uint8_t>(bytes)) / 2;
}

std::vector<uint8_t> ebus::data1c_2_byte(const double_t &value) {
  return int_2_byte(static_cast<uint8_t>(value * 2));
}

// DATA2B
double_t ebus::byte_2_data2b(const std::vector<uint8_t> &bytes) {
  return static_cast<double_t>(byte_2_int<int16_t>(bytes)) / 256;
}

std::vector<uint8_t> ebus::data2b_2_byte(const double_t &value) {
  return int_2_byte(static_cast<int16_t>(value * 256));
}

// DATA2C
double_t ebus::byte_2_data2c(const std::vector<uint8_t> &bytes) {
  return static_cast<double_t>(byte_2_int<int16_t>(bytes)) / 16;
}

std::vector<uint8_t> ebus::data2c_2_byte(const double_t &value) {
  return int_2_byte(static_cast<int16_t>(value * 16));
}

// FLOAT
double_t ebus::byte_2_float(const std::vector<uint8_t> &bytes) {
  return round_digits(static_cast<double_t>(byte_2_int<int16_t>(bytes)) / 1000,
                      3);
}

std::vector<uint8_t> ebus::float_2_byte(const double_t &value) {
  return int_2_byte(static_cast<int16_t>(round_digits(value * 1000, 3)));
}

// STRING
const std::string ebus::byte_2_string(const std::vector<uint8_t> &vec) {
  std::ostringstream ostr;

  for (size_t i = 0; i < vec.size(); i++) ostr << static_cast<char>(vec[i]);

  return ostr.str();
}

const std::vector<uint8_t> ebus::string_2_byte(const std::string &str) {
  std::vector<uint8_t> result;

  for (size_t i = 0; i < str.size(); i++) result.push_back(uint8_t(str[i]));

  return result;
}
