/*
 * Copyright (C) 2025 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <ebus/utils.hpp>

std::string ebus::toString(uint8_t byte) {
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string s;
  s.push_back(hex_chars[byte >> 4]);
  s.push_back(hex_chars[byte & 0xf]);
  return s;
}

std::vector<uint8_t> ebus::toVector(const std::string& str) {
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
