/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus::detail {

/**
 * Suspends execution for a given number of milliseconds.
 */
void sleepMilli(uint32_t ms);

/**
 * Suspends execution for a given number of microseconds.
 */
void sleepMicro(uint32_t us);

}  // namespace ebus::detail