/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ebus {

/**
 * Returns the number of zero bits in a byte.
 */
constexpr uint8_t countZeroBits(uint8_t byte) {
#if defined(__GNUC__) || defined(__clang__)
  return static_cast<uint8_t>(8 - __builtin_popcount(byte));
#else
  uint8_t count = 0;
  for (int i = 0; i < 8; i++) {
    if (!(byte & (1 << i))) {
      count++;
    }
  }
  return count;
#endif
}

/**
 * Swaps the byte order of an integral value.
 */
template <typename T>
constexpr T swapEndian(T val) {
  static_assert(std::is_integral_v<T>, "swapEndian requires integral type");
  if constexpr (sizeof(T) == 1) {
    return val;
  } else if constexpr (sizeof(T) == 2) {
    return static_cast<T>((val >> 8) | (val << 8));
  } else if constexpr (sizeof(T) == 4) {
    return ((val >> 24) & 0xff) | ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) | ((val << 24) & 0xff000000);
  } else {
    return val;
  }
}

namespace detail {
/**
 * Helper function to calculate a single CRC table entry at compile time.
 */
constexpr uint8_t calculateCrcEntry(uint8_t index) {
  uint8_t crc = index;
  for (int i = 0; i < 8; ++i) {
    if (crc & 0x80) {
      crc = static_cast<uint8_t>((crc << 1) ^ 0x9b);
    } else {
      crc = static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

/**
 * Generates the CRC table as a constexpr array.
 */
constexpr std::array<uint8_t, 256> generateCrcTable() {
  std::array<uint8_t, 256> table{};
  for (std::size_t i = 0; i < 256; ++i) {
    table[i] = calculateCrcEntry(static_cast<uint8_t>(i));
  }
  return table;
}

}  // namespace detail

inline constexpr std::array<uint8_t, 256> crc_table =
    detail::generateCrcTable();

/**
 * Calculates the eBUS 8-bit CRC using the 0x9b polynomial.
 */
constexpr uint8_t calcCRC(uint8_t byte, uint8_t init) {
  return static_cast<uint8_t>(crc_table[init] ^ byte);
}

}  // namespace ebus
