/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include "esp/bus_esp.hpp"
namespace ebus::detail::platform {
using Bus = BusEsp;
}  // namespace ebus::detail::platform
#elif defined(POSIX)
#include "posix/bus_posix.hpp"
namespace ebus::detail::platform {
using Bus = BusPosix;
}  // namespace ebus::detail::platform
#endif
