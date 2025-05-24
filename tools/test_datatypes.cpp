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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Datatypes.hpp"
#include "Sequence.hpp"

// Examples 1 byte
std::vector<uint8_t> vecB1 = {0x00, 0x01, 0x64, 0x7f, 0x80,
                              0x81, 0xc8, 0xfe, 0xff};

// Examples 2 bytes
std::vector<uint8_t> vecB2 = {0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0x00,
                              0xff, 0xf0, 0xff, 0x00, 0x80, 0x01, 0x80,
                              0xff, 0x7f, 0x65, 0x02, 0x77, 0x02};

// Examples 4 bytes
std::vector<uint8_t> vecB4 = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                              0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
                              0xf0, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x80,
                              0x01, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
                              0x65, 0x02, 0x7b, 0xcc, 0x77, 0x02, 0x2d, 0xe4};

void printLine(const std::vector<uint8_t>& bytes,
               const std::vector<uint8_t>& result, const std::string& value) {
  std::cout << "bytes " << ebus::to_string(bytes) << " encode "
            << ebus::to_string(result) << " decode " << value << std::endl;
}

void compareBCD(const std::vector<uint8_t>& bytes, const bool& print) {
  uint8_t value = ebus::byte_2_bcd(bytes);
  std::vector<uint8_t> result(1, ebus::bcd_2_byte(value)[0]);

  if ((value != 0xff && bytes != result) || print)
    printLine(bytes, result, std::to_string(value));
}

void compareUINT8(const std::vector<uint8_t>& bytes, const bool& print) {
  uint8_t value = ebus::byte_2_uint8(bytes);
  std::vector<uint8_t> result = ebus::uint8_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareINT8(const std::vector<uint8_t>& bytes, const bool& print) {
  int8_t value = ebus::byte_2_int8(bytes);
  std::vector<uint8_t> result = ebus::int8_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareDATA1B(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data1b(bytes);
  std::vector<uint8_t> result = ebus::data1b_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareDATA1C(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data1c(bytes);
  std::vector<uint8_t> result = ebus::data1c_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareUINT16(const std::vector<uint8_t>& bytes, const bool& print) {
  uint16_t value = ebus::byte_2_uint16(bytes);
  std::vector<uint8_t> result = ebus::uint16_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareINT16(const std::vector<uint8_t>& bytes, const bool& print) {
  int16_t value = ebus::byte_2_int16(bytes);
  std::vector<uint8_t> result = ebus::int16_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareDATA2B(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data2b(bytes);
  std::vector<uint8_t> result = ebus::data2b_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareDATA2C(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data2c(bytes);
  std::vector<uint8_t> result = ebus::data2c_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareFLOAT(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_float(bytes);
  std::vector<uint8_t> result = ebus::float_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareUINT32(const std::vector<uint8_t>& bytes, const bool& print) {
  uint32_t value = ebus::byte_2_uint32(bytes);
  std::vector<uint8_t> result = ebus::uint32_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void compareINT32(const std::vector<uint8_t>& bytes, const bool& print) {
  int32_t value = ebus::byte_2_int32(bytes);
  std::vector<uint8_t> result = ebus::int32_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

void examplesB1(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Examples " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (size_t i = 0; i < vecB1.size(); i++) {
    std::vector<uint8_t> src(1, vecB1[i]);
    function(src, print);
  }
}

void fullB1(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Full " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (int b0 = 0x00; b0 <= 0xff; b0++) {
    std::vector<uint8_t> src(1, uint8_t(b0));
    function(src, print);
  }
}

void testB1(const ebus::DataType& datatype, const bool& printExamples,
            const bool& printFull) {
  switch (datatype) {
    case ebus::DataType::BCD:
      examplesB1(datatype, printExamples, compareBCD);
      fullB1(datatype, printFull, compareBCD);
      break;
    case ebus::DataType::UINT8:
      examplesB1(datatype, printExamples, compareUINT8);
      fullB1(datatype, printFull, compareUINT8);
      break;
    case ebus::DataType::INT8:
      examplesB1(datatype, printExamples, compareINT8);
      fullB1(datatype, printFull, compareINT8);
      break;
    case ebus::DataType::DATA1B:
      examplesB1(datatype, printExamples, compareDATA1B);
      fullB1(datatype, printFull, compareDATA1B);
      break;
    case ebus::DataType::DATA1C:
      examplesB1(datatype, printExamples, compareDATA1C);
      fullB1(datatype, printFull, compareDATA1C);
      break;
    default:
      break;
  }
}

void examplesB2(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Examples " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (size_t i = 0; i < vecB2.size(); i += 2) {
    std::vector<uint8_t> src{vecB2[i], vecB2[i + 1]};
    function(src, print);
  }
}

void fullB2(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Full " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (int b1 = 0x00; b1 <= 0xff; b1++) {
    for (int b0 = 0x00; b0 <= 0xff; b0++) {
      std::vector<uint8_t> src{uint8_t(b0), uint8_t(b1)};
      function(src, print);
    }
  }
}

void testB2(const ebus::DataType& datatype, const bool& printExamples,
            const bool& printFull) {
  switch (datatype) {
    case ebus::DataType::UINT16:
      examplesB2(datatype, printExamples, compareUINT16);
      fullB2(datatype, printFull, compareUINT16);
      break;
    case ebus::DataType::INT16:
      examplesB2(datatype, printExamples, compareINT16);
      fullB2(datatype, printFull, compareINT16);
      break;
    case ebus::DataType::DATA2B:
      examplesB2(datatype, printExamples, compareDATA2B);
      fullB2(datatype, printFull, compareDATA2B);
      break;
    case ebus::DataType::DATA2C:
      examplesB2(datatype, printExamples, compareDATA2C);
      fullB2(datatype, printFull, compareDATA2C);
      break;
    case ebus::DataType::FLOAT:
      examplesB2(datatype, printExamples, compareFLOAT);
      fullB2(datatype, printFull, compareFLOAT);
      break;
    default:
      break;
  }
}

void examplesB4(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Examples " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (size_t i = 0; i < vecB4.size(); i += 4) {
    std::vector<uint8_t> src{vecB4[i], vecB4[i + 1], vecB4[i + 2],
                             vecB4[i + 3]};
    function(src, print);
  }
}

void fullB4(
    const ebus::DataType& datatype, const bool& print,
    std::function<void(const std::vector<uint8_t>& bytes, const bool& print)>
        function) {
  std::cout << std::endl
            << "Full " << datatype_2_string(datatype) << std::endl
            << std::endl;

  for (int b3 = 0x00; b3 <= 0xff; b3++) {
    for (int b2 = 0x00; b2 <= 0xff; b2++) {
      for (int b1 = 0x00; b1 <= 0xff; b1++) {
        for (int b0 = 0x00; b0 <= 0xff; b0++) {
          std::vector<uint8_t> src{uint8_t(b0), uint8_t(b1), uint8_t(b2),
                                   uint8_t(b3)};
          function(src, print);
        }
      }
    }
  }
}

void testB4(const ebus::DataType& datatype, const bool& printExamples,
            const bool& printFull) {
  switch (datatype) {
    case ebus::DataType::UINT32:
      examplesB4(datatype, printExamples, compareUINT32);
      // fullB4(datatype, printFull, compareUINT32);
      break;
    case ebus::DataType::INT32:
      examplesB4(datatype, printExamples, compareINT32);
      // fullB4(datatype, printFull, compareINT32);
      break;
    default:
      break;
  }
}

void testSTRING() {
  std::string ehp00_string = "EHP00";
  std::vector<uint8_t> ehp00_byte = {0x45, 0x48, 0x50, 0x30, 0x30};

  std::cout << std::endl << std::endl << "Examples STRING" << std::endl;

  std::cout << std::endl
            << "from STRING " << ehp00_string << " to BYTE "
            << ebus::to_string(ebus::string_2_byte(ehp00_string)) << std::endl;

  std::cout << std::endl
            << "from BYTE " << ebus::to_string(ehp00_byte) << " to STRING "
            << ebus::byte_2_string(ehp00_byte) << std::endl;
}

void testERROR() {
  std::cout << std::endl << std::endl << "Examples ERROR" << std::endl;

  std::cout << std::endl
            << "DATA2c "
            << (ebus::string_2_datatype("DATA2c") == ebus::DataType::ERROR
                    ? "not definded"
                    : "defined")
            << std::endl;

  std::cout << std::endl
            << "DATA2C "
            << (ebus::string_2_datatype("DATA2C") == ebus::DataType::ERROR
                    ? "not definded"
                    : "defined")
            << std::endl;
}

int main() {
  testB1(ebus::DataType::BCD, false, false);
  testB1(ebus::DataType::UINT8, false, false);
  testB1(ebus::DataType::INT8, false, false);
  testB1(ebus::DataType::DATA1B, false, false);
  testB1(ebus::DataType::DATA1C, false, false);

  testB2(ebus::DataType::UINT16, true, false);
  testB2(ebus::DataType::INT16, true, false);
  testB2(ebus::DataType::DATA2B, true, false);
  testB2(ebus::DataType::DATA2C, true, false);
  testB2(ebus::DataType::FLOAT, true, false);

  testB4(ebus::DataType::UINT32, true, false);
  testB4(ebus::DataType::INT32, true, false);

  testSTRING();

  testERROR();

  return EXIT_SUCCESS;
}
