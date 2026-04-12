/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstring>
#include <ebus/data_types.hpp>
#include <ebus/utils.hpp>
#include <limits>
#include <map>
#include <utility>

std::map<ebus::DataType, const char*> data_type_names = {
    {ebus::DataType::error, "ERROR"},     {ebus::DataType::bcd, "BCD"},
    {ebus::DataType::uint8, "UINT8"},     {ebus::DataType::int8, "INT8"},
    {ebus::DataType::data1b, "DATA1B"},   {ebus::DataType::data1c, "DATA1C"},
    {ebus::DataType::uint16, "UINT16"},   {ebus::DataType::uint16r, "UINT16R"},
    {ebus::DataType::int16, "INT16"},     {ebus::DataType::int16r, "INT16R"},
    {ebus::DataType::data2b, "DATA2B"},   {ebus::DataType::data2br, "DATA2BR"},
    {ebus::DataType::data2c, "DATA2C"},   {ebus::DataType::data2cr, "DATA2CR"},
    {ebus::DataType::uint32, "UINT32"},   {ebus::DataType::uint32r, "UINT32R"},
    {ebus::DataType::int32, "INT32"},     {ebus::DataType::int32r, "INT32R"},
    {ebus::DataType::float_val, "FLOAT"}, {ebus::DataType::floatr, "FLOATR"},
    {ebus::DataType::char1, "CHAR1"},     {ebus::DataType::char2, "CHAR2"},
    {ebus::DataType::char3, "CHAR3"},     {ebus::DataType::char4, "CHAR4"},
    {ebus::DataType::char5, "CHAR5"},     {ebus::DataType::char6, "CHAR6"},
    {ebus::DataType::char7, "CHAR7"},     {ebus::DataType::char8, "CHAR8"},
    {ebus::DataType::hex1, "HEX1"},       {ebus::DataType::hex2, "HEX2"},
    {ebus::DataType::hex3, "HEX3"},       {ebus::DataType::hex4, "HEX4"},
    {ebus::DataType::hex5, "HEX5"},       {ebus::DataType::hex6, "HEX6"},
    {ebus::DataType::hex7, "HEX7"},       {ebus::DataType::hex8, "HEX8"}};

const char* ebus::dataTypeToString(const ebus::DataType& data_type) {
  return data_type_names[data_type];
}

ebus::DataType ebus::stringToDataType(const char* str) {
  DataType data_type = DataType::error;

  const auto it =
      std::find_if(data_type_names.begin(), data_type_names.end(),
                   [&str](const std::pair<DataType, const char*>& item) {
                     return strcmp(str, item.second) == 0;
                   });

  if (it != data_type_names.end()) data_type = it->first;

  return data_type;
}

size_t ebus::sizeOfDataType(const ebus::DataType& data_type) {
  size_t length = 0;

  switch (data_type) {
    case ebus::DataType::bcd:
    case ebus::DataType::uint8:
    case ebus::DataType::int8:
    case ebus::DataType::data1b:
    case ebus::DataType::data1c:
    case ebus::DataType::char1:
    case ebus::DataType::hex1:
      length = 1;
      break;
    case ebus::DataType::uint16:
    case ebus::DataType::uint16r:
    case ebus::DataType::int16:
    case ebus::DataType::int16r:
    case ebus::DataType::data2b:
    case ebus::DataType::data2br:
    case ebus::DataType::data2c:
    case ebus::DataType::data2cr:
    case ebus::DataType::char2:
    case ebus::DataType::hex2:
      length = 2;
      break;
    case ebus::DataType::char3:
    case ebus::DataType::hex3:
      length = 3;
      break;
    case ebus::DataType::uint32:
    case ebus::DataType::uint32r:
    case ebus::DataType::int32:
    case ebus::DataType::int32r:
    case ebus::DataType::float_val:
    case ebus::DataType::floatr:
    case ebus::DataType::char4:
    case ebus::DataType::hex4:
      length = 4;
      break;
    case ebus::DataType::char5:
    case ebus::DataType::hex5:
      length = 5;
      break;
    case ebus::DataType::char6:
    case ebus::DataType::hex6:
      length = 6;
      break;
    case ebus::DataType::char7:
    case ebus::DataType::hex7:
      length = 7;
      break;
    case ebus::DataType::char8:
    case ebus::DataType::hex8:
      length = 8;
      break;
    default:
      break;
  }
  return length;
}

bool ebus::typeOfDataType(const DataType& data_type) {
  bool numeric = true;

  switch (data_type) {
    case ebus::DataType::char1:
    case ebus::DataType::char2:
    case ebus::DataType::char3:
    case ebus::DataType::char4:
    case ebus::DataType::char5:
    case ebus::DataType::char6:
    case ebus::DataType::char7:
    case ebus::DataType::char8:
    case ebus::DataType::hex1:
    case ebus::DataType::hex2:
    case ebus::DataType::hex3:
    case ebus::DataType::hex4:
    case ebus::DataType::hex5:
    case ebus::DataType::hex6:
    case ebus::DataType::hex7:
    case ebus::DataType::hex8:
      numeric = false;
      break;
    default:
      break;
  }

  return numeric;
}

double_t ebus::roundDigits(const double_t& value, const uint8_t& digits) {
  double_t fractpart, intpart;
  fractpart = std::modf(value, &intpart);

  double_t decimals = std::pow(10, digits);

  return static_cast<double_t>(intpart) +
         std::round(fractpart * decimals) / decimals;
}

// BCD
uint8_t ebus::byteToBcd(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0xff;
  uint8_t value = bytes[0];
  if ((value & 0x0f) > 9 || ((value >> 4) & 0x0f) > 9) return 0xff;
  return (value >> 4) * 10 + (value & 0x0f);
}

std::vector<uint8_t> ebus::bcdToByte(const uint8_t& value) {
  if (value > 99) return {0xff};
  uint8_t bcd = ((value / 10) << 4) | (value % 10);
  return {bcd};
}

// void ebus::bcd_2_byte(const uint8_t& value, uint8_t* out) {
//   if (value > 99) { *out = 0xff; return; }
//   uint8_t bcd = ((value / 10) << 4) | (value % 10);
//   *out = bcd;
// }

// UINT8
uint8_t ebus::byteToUint8(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0;
  return bytes[0];
}

std::vector<uint8_t> ebus::uint8ToByte(const uint8_t& value) { return {value}; }

// INT8
int8_t ebus::byteToInt8(const std::vector<uint8_t>& bytes) {
  if (bytes.size() != 1) return 0;
  return static_cast<int8_t>(bytes[0]);
}

std::vector<uint8_t> ebus::int8ToByte(const int8_t& value) {
  return {static_cast<uint8_t>(value)};
}

// DATA1B
double_t ebus::byteToData1b(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byteToInt8(bytes));
}

std::vector<uint8_t> ebus::data1bToByte(const double_t& value) {
  return ebus::int8ToByte(static_cast<int8_t>(value));
}

// DATA1C
double_t ebus::byteToData1c(const std::vector<uint8_t>& bytes) {
  return static_cast<double_t>(ebus::byteToUint8(bytes)) / 2.0;
}

std::vector<uint8_t> ebus::data1cToByte(const double_t& value) {
  return ebus::uint8ToByte(static_cast<uint8_t>(value * 2.0));
}

// UINT16 + UINT16R
uint16_t ebus::byteToUint16(const std::vector<uint8_t>& bytes, Endian e) {
  if (bytes.size() != 2) return 0;
  if (e == Endian::little) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
  } else {
    return (static_cast<uint16_t>(bytes[0]) << 8) |
           static_cast<uint16_t>(bytes[1]);
  }
}

std::vector<uint8_t> ebus::uint16ToByte(const uint16_t& value, Endian e) {
  if (e == Endian::little) {
    return std::vector<uint8_t>{static_cast<uint8_t>(value & 0xff),
                                static_cast<uint8_t>((value >> 8) & 0xff)};
  } else {
    return std::vector<uint8_t>{static_cast<uint8_t>((value >> 8) & 0xff),
                                static_cast<uint8_t>(value & 0xff)};
  }
}

// INT16 + INT16R
int16_t ebus::byteToInt16(const std::vector<uint8_t>& bytes, Endian e) {
  if (bytes.size() != 2) return 0;
  uint32_t uval = ebus::byteToUint16(bytes, e);
  return static_cast<int32_t>(uval);
}

std::vector<uint8_t> ebus::int16ToByte(const int16_t& value, Endian e) {
  uint32_t uval = static_cast<uint32_t>(value);
  return ebus::uint16ToByte(uval, e);
}

// DATA2B + DATA2BR
double_t ebus::byteToData2b(const std::vector<uint8_t>& bytes, Endian e) {
  return static_cast<double_t>(ebus::byteToInt16(bytes, e)) / 256.0;
}

std::vector<uint8_t> ebus::data2bToByte(const double_t& value, Endian e) {
  return ebus::int16ToByte(static_cast<int16_t>(value * 256.0), e);
}

// DATA2C + DATA2CR
double_t ebus::byteToData2c(const std::vector<uint8_t>& bytes, Endian e) {
  return static_cast<double_t>(ebus::byteToInt16(bytes, e)) / 16.0;
}

std::vector<uint8_t> ebus::data2cToByte(const double_t& value, Endian e) {
  return ebus::int16ToByte(static_cast<int16_t>(value * 16.0), e);
}

// UINT32 + UINT32R
uint32_t ebus::byteToUint32(const std::vector<uint8_t>& bytes, Endian e) {
  if (bytes.size() != 4) return 0;
  if (e == Endian::little) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
  } else {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
  }
}

std::vector<uint8_t> ebus::uint32ToByte(const uint32_t& value, Endian e) {
  if (e == Endian::little) {
    return {static_cast<uint8_t>(value & 0xff),
            static_cast<uint8_t>((value >> 8) & 0xff),
            static_cast<uint8_t>((value >> 16) & 0xff),
            static_cast<uint8_t>((value >> 24) & 0xff)};
  } else {
    return {static_cast<uint8_t>((value >> 24) & 0xff),
            static_cast<uint8_t>((value >> 16) & 0xff),
            static_cast<uint8_t>((value >> 8) & 0xff),
            static_cast<uint8_t>(value & 0xff)};
  }
}

// INT32 + INT32R
int32_t ebus::byteToInt32(const std::vector<uint8_t>& bytes, Endian e) {
  if (bytes.size() != 4) return 0;
  uint32_t uval = ebus::byteToUint32(bytes, e);
  return static_cast<int32_t>(uval);
}

std::vector<uint8_t> ebus::int32ToByte(const int32_t& value, Endian e) {
  uint32_t uval = static_cast<uint32_t>(value);
  return ebus::uint32ToByte(uval, e);
}

// FLOAT (IEEE 754 single precision)
double_t ebus::byteToFloat(const std::vector<uint8_t>& bytes, Endian e) {
  if (bytes.size() != 4) return 0.0;
  uint32_t raw = ebus::byteToUint32(bytes, e);
  float value;
  std::memcpy(&value, &raw, sizeof(float));
  return static_cast<double_t>(value);
}

std::vector<uint8_t> ebus::floatToByte(const double_t& value, Endian e) {
  float f = static_cast<float>(value);
  uint32_t raw;
  std::memcpy(&raw, &f, sizeof(float));
  return ebus::uint32ToByte(raw, e);
}

// CHAR
std::string ebus::byteToChar(const std::vector<uint8_t>& vec) {
  return std::string(vec.begin(), vec.end());
}

std::vector<uint8_t> ebus::charToByte(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// HEX
std::string ebus::byteToHex(const std::vector<uint8_t>& vec) {
  return ebus::toString(vec);
}

std::vector<uint8_t> ebus::hexToByte(const std::string& str) {
  return ebus::toVector(str);
}
