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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Datatypes.h"

const std::string toString(const std::vector<uint8_t>& vec) {
  std::ostringstream ostr;

  for (size_t i = 0; i < vec.size(); i++)
    ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned>(vec[i]);

  return (ostr.str());
}

void printLine(const std::vector<uint8_t>& bytes,
               const std::vector<uint8_t>& result, const std::string& value) {
  std::cout << "bytes " << toString(bytes) << " encode " << toString(result)
            << " decode " << value << std::endl;
}

// BCD
void compareBCD(const std::vector<uint8_t>& bytes, const bool& print) {
  uint8_t value = ebus::byte_2_bcd(bytes);
  std::vector<uint8_t> result(1, ebus::bcd_2_byte(value)[0]);

  if ((value != 0xff && bytes != result) || print)
    printLine(bytes, result, std::to_string(value));
}

// UINT8
void compareUINT8(const std::vector<uint8_t>& bytes, const bool& print) {
  uint8_t value = ebus::byte_2_uint8(bytes);
  std::vector<uint8_t> result = ebus::uint8_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// INT8
void compareINT8(const std::vector<uint8_t>& bytes, const bool& print) {
  int8_t value = ebus::byte_2_int8(bytes);
  std::vector<uint8_t> result = ebus::int8_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// UINT16
void compareUINT16(const std::vector<uint8_t>& bytes, const bool& print) {
  uint16_t value = ebus::byte_2_uint16(bytes);
  std::vector<uint8_t> result = ebus::uint16_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// INT16
void compareINT16(const std::vector<uint8_t>& bytes, const bool& print) {
  int16_t value = ebus::byte_2_int16(bytes);
  std::vector<uint8_t> result = ebus::int16_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// DATA1B
void compareDATA1B(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data1b(bytes);
  std::vector<uint8_t> result = ebus::data1b_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// DATA1C
void compareDATA1C(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data1c(bytes);
  std::vector<uint8_t> result = ebus::data1c_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// DATA2B
void compareDATA2B(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data2b(bytes);
  std::vector<uint8_t> result = ebus::data2b_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// DATA2C
void compareDATA2C(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_data2c(bytes);
  std::vector<uint8_t> result = ebus::data2c_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

// FLOAT
void compareFLOAT(const std::vector<uint8_t>& bytes, const bool& print) {
  double_t value = ebus::byte_2_float(bytes);
  std::vector<uint8_t> result = ebus::float_2_byte(value);

  if (bytes != result || print) printLine(bytes, result, std::to_string(value));
}

int main() {
  std::vector<uint8_t> b1 = {0x00, 0x01, 0x64, 0x7f, 0x80,
                             0x81, 0xc8, 0xfe, 0xff};

  std::vector<uint8_t> b2 = {0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0x00,
                             0xff, 0xf0, 0xff, 0x00, 0x80, 0x01, 0x80,
                             0xff, 0x7f, 0x65, 0x02, 0x77, 0x02};

  // BCD
  std::cout << std::endl
            << std::endl
            << "Examples BCD" << std::endl
            << std::endl;

  for (size_t i = 0; i < b1.size(); i++) {
    std::vector<uint8_t> src(1, b1[i]);

    compareBCD(src, true);
  }

  std::cout << std::endl << "Check range BCD" << std::endl;

  for (int low = 0x00; low <= 0xff; low++) {
    std::vector<uint8_t> src(1, uint8_t(low));

    compareBCD(src, false);
  }

  // UINT8
  std::cout << std::endl
            << std::endl
            << "Examples UINT8" << std::endl
            << std::endl;

  for (size_t i = 0; i < b1.size(); i++) {
    std::vector<uint8_t> src(1, b1[i]);

    compareUINT8(src, true);
  }

  std::cout << std::endl << "Check range UINT8" << std::endl;

  for (int low = 0x00; low <= 0xff; low++) {
    std::vector<uint8_t> src(1, uint8_t(low));

    compareUINT8(src, false);
  }

  // INT8
  std::cout << std::endl
            << std::endl
            << "Examples INT8" << std::endl
            << std::endl;

  for (size_t i = 0; i < b1.size(); i++) {
    std::vector<uint8_t> src(1, b1[i]);

    compareINT8(src, true);
  }

  std::cout << std::endl << "Check range INT8" << std::endl;

  for (int low = 0x00; low <= 0xff; low++) {
    std::vector<uint8_t> src(1, uint8_t(low));

    compareINT8(src, false);
  }

  // UINT16
  std::cout << std::endl
            << std::endl
            << "Examples UINT16" << std::endl
            << std::endl;

  for (size_t i = 0; i < b2.size(); i += 2) {
    std::vector<uint8_t> src{b2[i], b2[i + 1]};

    compareUINT16(src, true);
  }

  std::cout << std::endl << "Check range UINT16" << std::endl;

  for (int high = 0x00; high <= 0xff; high++) {
    for (int low = 0x00; low <= 0xff; low++) {
      std::vector<uint8_t> src{uint8_t(low), uint8_t(high)};

      compareUINT16(src, false);
    }
  }

  // INT16
  std::cout << std::endl
            << std::endl
            << "Examples INT16" << std::endl
            << std::endl;

  for (size_t i = 0; i < b2.size(); i += 2) {
    std::vector<uint8_t> src{b2[i], b2[i + 1]};

    compareINT16(src, true);
  }

  std::cout << std::endl << "Check range INT16" << std::endl;

  for (int high = 0x00; high <= 0xff; high++) {
    for (int low = 0x00; low <= 0xff; low++) {
      std::vector<uint8_t> src{uint8_t(low), uint8_t(high)};

      compareINT16(src, false);
    }
  }

  // DATA1B
  std::cout << std::endl
            << std::endl
            << "Examples DATA1B" << std::endl
            << std::endl;

  for (size_t i = 0; i < b1.size(); i++) {
    std::vector<uint8_t> src(1, b1[i]);

    compareDATA1B(src, true);
  }

  std::cout << std::endl << "Check range DATA1B" << std::endl;

  for (int low = 0x00; low <= 0xff; low++) {
    std::vector<uint8_t> src(1, uint8_t(low));

    compareDATA1B(src, false);
  }

  // DATA1C
  std::cout << std::endl
            << std::endl
            << "Examples DATA1C" << std::endl
            << std::endl;

  for (size_t i = 0; i < b1.size(); i++) {
    std::vector<uint8_t> src(1, b1[i]);

    compareDATA1C(src, true);
  }

  std::cout << std::endl << "Check range DATA1C" << std::endl;

  for (int low = 0x00; low <= 0xff; low++) {
    std::vector<uint8_t> src(1, uint8_t(low));

    compareDATA1C(src, false);
  }

  // DATA2B
  std::cout << std::endl
            << std::endl
            << "Examples DATA2B" << std::endl
            << std::endl;

  for (size_t i = 0; i < b2.size(); i += 2) {
    std::vector<uint8_t> src{b2[i], b2[i + 1]};

    compareDATA2B(src, true);
  }

  std::cout << std::endl << "Check range DATA2B" << std::endl;

  for (int high = 0x00; high <= 0xff; high++) {
    for (int low = 0x00; low <= 0xff; low++) {
      std::vector<uint8_t> src{uint8_t(low), uint8_t(high)};

      compareDATA2B(src, false);
    }
  }

  // DATA2C
  std::cout << std::endl
            << std::endl
            << "Examples DATA2C" << std::endl
            << std::endl;

  for (size_t i = 0; i < b2.size(); i += 2) {
    std::vector<uint8_t> src{b2[i], b2[i + 1]};

    compareDATA2C(src, true);
  }

  std::cout << std::endl << "Check range DATA2C" << std::endl;

  for (int high = 0x00; high <= 0xff; high++) {
    for (int low = 0x00; low <= 0xff; low++) {
      std::vector<uint8_t> src{uint8_t(low), uint8_t(high)};

      compareDATA2C(src, false);
    }
  }

  // FLOAT
  std::cout << std::endl
            << std::endl
            << "Examples FLOAT" << std::endl
            << std::endl;

  for (size_t i = 0; i < b2.size(); i += 2) {
    std::vector<uint8_t> src{b2[i], b2[i + 1]};

    compareFLOAT(src, true);
  }

  std::cout << std::endl << "Check range FLOAT" << std::endl;

  for (int high = 0x00; high <= 0xff; high++) {
    for (int low = 0x00; low <= 0xff; low++) {
      std::vector<uint8_t> src{uint8_t(low), uint8_t(high)};

      compareFLOAT(src, false);
    }
  }

  // ERROR
  std::cout << std::endl << std::endl << "Examples ERROR" << std::endl;

  std::cout << std::endl
            << "DATA2c "
            << (ebus::string2datatype("DATA2c") == ebus::Datatype::ERROR
                    ? "not definded"
                    : "defined")
            << std::endl;

  std::cout << std::endl
            << "DATA2C "
            << (ebus::string2datatype("DATA2C") == ebus::Datatype::ERROR
                    ? "not definded"
                    : "defined")
            << std::endl;

  return (0);
}
