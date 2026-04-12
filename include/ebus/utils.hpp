/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ebus/definitions.hpp"

namespace ebus {

// --- Address Logic ---

/**
 * Checks if a byte conforms to the eBUS master address rules (Spec 6.2.2.1).
 * Valid values x satisfy ((x + 1) & x) == 0 for both nibbles (value <= 15).
 */
constexpr bool isMaster(uint8_t byte) {
  return ((((byte >> 4) & 0x0f) + 1) & ((byte >> 4) & 0x0f)) == 0 &&
         (((byte & 0x0f) + 1) & (byte & 0x0f)) == 0;
}

/**
 * Checks if a byte is a slave address (not master, not SYN, EXT, or BROADCAST).
 */
constexpr bool isSlave(uint8_t byte) {
  return !isMaster(byte) && byte != sym_syn && byte != sym_ext &&
         byte != sym_broad;
}

/**
 * Checks if a byte is a valid target address (not SYN or EXT).
 */
constexpr bool isTarget(uint8_t byte) {
  return byte != sym_syn && byte != sym_ext;
}

constexpr uint8_t masterOf(uint8_t byte) {
  return isMaster(static_cast<uint8_t>(byte - 5))
             ? static_cast<uint8_t>(byte - 5)
             : byte;
}

constexpr uint8_t slaveOf(uint8_t byte) {
  return isMaster(byte) ? static_cast<uint8_t>(byte + 5) : byte;
}

// --- Hex and String Conversion ---
std::string toString(uint8_t byte);

/**
 * Converts any byte container to a hex string.
 */
template <typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>>>
std::string toString(const T& container) {
  std::ostringstream ostr;
  for (auto b : container)
    ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned>(b);
  return ostr.str();
}

std::vector<uint8_t> toVector(const std::string& str);

// --- Vector Helpers ---

template <typename T>
std::vector<uint8_t> range(const T& container, size_t index, size_t len) {
  if (index >= container.size()) return {};
  size_t end_idx = std::min(index + len, container.size());
  return std::vector<uint8_t>(container.begin() + index,
                              container.begin() + end_idx);
}

template <typename T, typename U>
bool contains(const T& container, const U& search) {
  if (std::empty(search) || std::empty(container) ||
      std::size(search) > std::size(container))
    return false;
  return std::search(container.begin(), container.end(), search.begin(),
                     search.end()) != container.end();
}

/**
 * Overload for contains to support brace-enclosed initializer lists.
 */
template <typename T>
bool contains(const T& container, std::initializer_list<uint8_t> search) {
  return contains<T, std::initializer_list<uint8_t>>(container, search);
}

template <typename T, typename U>
bool matches(const T& container, const U& search, size_t index = 0) {
  if (std::empty(search)) return true;
  if (index + search.size() > container.size()) return false;
  return std::equal(search.begin(), search.end(), container.begin() + index);
}

/**
 * Overload for matches to support brace-enclosed initializer lists.
 */
template <typename T>
bool matches(const T& container, std::initializer_list<uint8_t> search,
             size_t index = 0) {
  return matches<T, std::initializer_list<uint8_t>>(container, search, index);
}

// --- Protocol Math ---

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