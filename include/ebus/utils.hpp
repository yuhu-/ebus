/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ebus/addressing.hpp"
#include "ebus/definitions.hpp"
#include "ebus/protocol_math.hpp"

namespace ebus {

// --- Hex and String Conversion ---
std::string toString(uint8_t byte);

/**
 * Converts any byte container to a hex string.
 */
template <typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>>>
std::string toString(const T& container) {
  if (std::empty(container)) return {};
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string res;
  res.reserve(std::size(container) * 2);
  for (auto b : container) {
    uint8_t byte = static_cast<uint8_t>(b);
    res.push_back(hex_chars[byte >> 4]);
    res.push_back(hex_chars[byte & 0xf]);
  }
  return res;
}

inline std::string byteToChar(ByteView view) {
  return std::string(reinterpret_cast<const char*>(view.data()), view.size());
}

inline std::string byteToHex(ByteView view) { return toString(view); }

std::vector<uint8_t> toVector(const std::string& str);

/**
 * Rounds a floating point value to a specific number of decimal places.
 */
inline double roundDigits(double value, uint8_t digits) noexcept {
  double decimals = std::pow(10, digits);
  return std::round(value * decimals) / decimals;
}

// --- Vector Helpers ---

/**
 * Returns a non-owning ByteView of a range within a container.
 */
template <typename T>
ByteView range(const T& container, size_t index, size_t len) {
  if (index >= container.size()) return {};
  size_t count = std::min(len, container.size() - index);
  return ByteView(container.data() + index, count);
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

}  // namespace ebus