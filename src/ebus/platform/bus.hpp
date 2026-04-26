/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP32)
#include "freertos/bus_freertos.hpp"
namespace ebus::detail {
using Bus = BusFreeRtos;
}  // namespace ebus::detail
#elif defined(POSIX)
#include "posix/bus_posix.hpp"
namespace ebus::detail {
using Bus = BusPosix;
}  // namespace ebus::detail
#endif
