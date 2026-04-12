/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

/**
 * Sequence class that represents a sequence of bytes in the eBUS protocol. It
 * provides methods for constructing sequences from vectors, comparing
 * sequences, calculating CRC, and converting between extended and reduced
 * formats. The class handles byte stuffing for the special characters 0xaa and
 * 0xa9 as defined in the eBUS specification.
 *
 * (reduced) 0xaa <-> 0xa9 0x01 (extended)
 * (reduced) 0xa9 <-> 0xa9 0x00 (extended)
 */
class Sequence {
 public:
  Sequence() = default;
  Sequence(const Sequence& sequence, const size_t index, size_t len = 0);

  void assign(const std::vector<uint8_t>& vec, const bool extended = true);
  void assign(const Sequence& other, size_t index, size_t len = 0);

  void pushBack(const uint8_t byte, const bool extended = true);

  bool operator==(const Sequence& other) const;
  bool operator!=(const Sequence& other) const;
  void append(const Sequence& other);

  const uint8_t& operator[](const size_t index) const;
  const std::vector<uint8_t> range(const size_t index, const size_t len) const;

  size_t size() const;

  void clear();
  bool isExtended() const { return extended_; }

  uint8_t crc() const;

  void extend();
  void reduce();

  const std::string toString() const;
  const std::vector<uint8_t>& toVector() const;

 private:
  std::vector<uint8_t> sequence_;
  bool extended_ = false;
};

}  // namespace ebus
