/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include "freertos/queue_freertos.hpp"
namespace ebus::detail::platform {
template <typename T>
using Queue = QueueFreeRtos<T>;
}  // namespace ebus::detail::platform
#elif defined(POSIX)
#include "posix/queue_posix.hpp"
namespace ebus::detail::platform {
template <typename T>
using Queue = QueuePosix<T>;
}  // namespace ebus::detail::platform
#endif
