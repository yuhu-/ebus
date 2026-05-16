/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ebus/protocol_math.hpp"
#include "ebus/types.hpp"

namespace ebus {

// --- Hex and String Conversion ---

/**
 * Escapes a string for use in a JSON value.
 */
inline std::string escapeJson(const std::string& s) {
  std::string res;
  res.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '"':
        res += "\\\"";
        break;
      case '\\':
        res += "\\\\";
        break;
      case '\b':
        res += "\\b";
        break;
      case '\f':
        res += "\\f";
        break;
      case '\n':
        res += "\\n";
        break;
      case '\r':
        res += "\\r";
        break;
      case '\t':
        res += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static const char hex[] = "0123456789abcdef";
          res += "\\u00";
          res += hex[(static_cast<unsigned char>(c) >> 4) & 0xf];
          res += hex[static_cast<unsigned char>(c) & 0xf];
        } else {
          res += c;
        }
    }
  }
  return res;
}

inline std::string toString(uint8_t byte) {
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string s;
  s.push_back(hex_chars[byte >> 4]);
  s.push_back(hex_chars[byte & 0xf]);
  return s;
}

/**
 * Converts any byte container to a hex string.
 */
template <typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>>>
std::string toString(const T& container) {
  if (std::empty(container)) return {};
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string res;
  res.reserve(container.size() * 2);
  for (auto b : container) {
    uint8_t byte = static_cast<uint8_t>(b);
    res.push_back(hex_chars[byte >> 4]);
    res.push_back(hex_chars[byte & 0xf]);
  }
  return res;
}

inline std::string byteToChar(ByteView data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

inline std::string byteToHex(ByteView data) { return toString(data); }

inline std::vector<uint8_t> toVector(const std::string& str) {
  if (str.empty()) return {};
  std::vector<uint8_t> result;
  result.reserve(str.size() / 2);
  for (size_t i = 0; i + 1 < str.size(); i += 2) {
    uint8_t value = 0;
    auto [ptr, ec] =
        std::from_chars(str.data() + i, str.data() + i + 2, value, 16);
    if (ec == std::errc{}) {
      result.push_back(value);
    }
  }
  return result;
}

/**
 * Converts a non-owning ByteView to an owning std::vector.
 */
inline std::vector<uint8_t> toVector(ByteView data) {
  return {data.begin(), data.end()};
}

/**
 * Rounds a floating point value to a specific number of decimal places.
 */
inline float roundDigits(float value, uint8_t digits) noexcept {
  float decimals = std::pow(10.0f, static_cast<float>(digits));
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