/*
 * Copyright (C) 2025 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/utils.hpp>

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

}  // namespace ebus
