/*
 * Copyright (C) 2012-2025 Roland Jax
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

// This class implements basic routines for sequence handling in accordance with
// the ebus specification, in particular the reduction and extension
//
// (reduced) 0xaa <-> 0xa9 0x01 (extended)
// (reduced) 0xa9 <-> 0xa9 0x00 (extended)

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

class Sequence {
 public:
  Sequence() = default;
  Sequence(const Sequence &seq, const size_t index, size_t len = 0);

  void assign(const std::vector<uint8_t> &vec, const bool extended = true);

  void push_back(const uint8_t byte, const bool extended = true);

  const uint8_t &operator[](const size_t index) const;
  const std::vector<uint8_t> range(const size_t index, const size_t len) const;

  size_t size() const;

  void clear();

  uint8_t crc();

  void extend();
  void reduce();

  const std::string to_string() const;
  const std::vector<uint8_t> &to_vector() const;

 private:
  std::vector<uint8_t> m_seq;

  bool m_extended = false;
};

}  // namespace ebus
