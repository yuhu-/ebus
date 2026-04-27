/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include "freertos/bus_freertos.hpp"
namespace ebus::detail::platform {
using Bus = BusFreeRtos;
}  // namespace ebus::detail::platform
#elif defined(POSIX)
#include "posix/bus_posix.hpp"
namespace ebus::detail::platform {
using Bus = BusPosix;
}  // namespace ebus::detail::platform
#endif
