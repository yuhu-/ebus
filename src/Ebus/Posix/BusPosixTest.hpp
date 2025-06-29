/*
 * Copyright (C) 2025 Roland Jax
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

#pragma once

#include <cstddef>
#include <iostream>
#include <vector>

#include "../Common.hpp"

namespace ebus {

// This class simulates a POSIX bus for testing purposes.
// It provides methods to write and read bytes, and check available bytes.
// In a real implementation, this would interface with a hardware bus.
class BusPosixTest {
 public:
  // Simulate writing a byte to the bus
  void writeByte(const uint8_t byte) {
    written_bytes.push_back(byte);
    std::cout << "<- write: " << ebus::to_string(byte) << std::endl;
  }

  // Simulate checking available bytes on the bus
  size_t available() const {
    // std::cout << "Checking available bytes: " << std::endl;
    return 0;
  }

  // Get all written bytes as a hex string
  std::string getWrittenBytesString() const {
    return ebus::to_string(written_bytes);
  }

  // Optionally, get the raw vector for further inspection
  const std::vector<uint8_t>& getWrittenBytes() const { return written_bytes; }

  // Clear the collected bytes (for repeated tests)
  void clearWrittenBytes() { written_bytes.clear(); }

 private:
  std::vector<uint8_t> written_bytes;
};

}  // namespace ebus
