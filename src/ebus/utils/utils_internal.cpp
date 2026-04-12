/*
 * Copyright (C) 2025 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "utils/utils_internal.hpp"

namespace ebus {

uint8_t countZeroBits(uint8_t byte) {
  uint8_t count = 0;
  for (int i = 0; i < 8; i++) {
    if (!(byte & (1 << i))) {
      count++;
    }
  }
  return count;
}

}  // namespace ebus
