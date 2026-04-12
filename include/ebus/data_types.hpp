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

enum class Endian { little, big };

enum class DataType {
  error = -1,
  bcd,
  uint8,
  int8,
  data1b,
  data1c,
  uint16,
  uint16r,
  int16,
  int16r,
  data2b,
  data2br,
  data2c,
  data2cr,
  uint32,
  uint32r,
  int32,
  int32r,
  float_val,
  floatr,
  char1,
  char2,
  char3,
  char4,
  char5,
  char6,
  char7,
  char8,
  hex1,
  hex2,
  hex3,
  hex4,
  hex5,
  hex6,
  hex7,
  hex8
};

const char* dataTypeToString(const DataType& datatype);
DataType stringToDataType(const char* str);
size_t sizeOfDataType(const DataType& datatype);
bool typeOfDataType(const DataType& datatype);
double_t roundDigits(const double_t& value, const uint8_t& digits);

uint8_t byteToBcd(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> bcdToByte(const uint8_t& value);

uint8_t byteToUint8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> uint8ToByte(const uint8_t& value);

int8_t byteToInt8(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> int8ToByte(const int8_t& value);

double_t byteToData1b(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1bToByte(const double_t& value);

double_t byteToData1c(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> data1cToByte(const double_t& value);

uint16_t byteToUint16(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> uint16ToByte(const uint16_t& value, Endian e);

int16_t byteToInt16(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> int16ToByte(const int16_t& value, Endian e);

double_t byteToData2b(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> data2bToByte(const double_t& value, Endian e);

double_t byteToData2c(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> data2cToByte(const double_t& value, Endian e);

uint32_t byteToUint32(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> uint32ToByte(const uint32_t& value, Endian e);

int32_t byteToInt32(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> int32ToByte(const int32_t& value, Endian e);

double_t byteToFloat(const std::vector<uint8_t>& bytes, Endian e);
std::vector<uint8_t> floatToByte(const double_t& value, Endian e);

std::string byteToChar(const std::vector<uint8_t>& vec);
std::vector<uint8_t> charToByte(const std::string& str);

std::string byteToHex(const std::vector<uint8_t>& vec);
std::vector<uint8_t> hexToByte(const std::string& str);

}  // namespace ebus