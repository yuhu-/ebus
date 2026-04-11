/*
 * Copyright (C) 2025 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/Utils.hpp>

namespace ebus {

/**
 * Returns the number of zero bits in a byte.
 */
uint8_t countZeroBits(uint8_t byte);

void sleep_ms(uint32_t ms);

}  // namespace ebus
