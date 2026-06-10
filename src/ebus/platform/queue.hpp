/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include "esp/queue_esp.hpp"
namespace ebus::detail::platform {
template <typename T>
class Queue : public QueueEsp<T> {
 public:
  using QueueEsp<T>::QueueEsp;
};
}  // namespace ebus::detail::platform
#elif defined(POSIX)
#include "posix/queue_posix.hpp"
namespace ebus::detail::platform {
template <typename T>
class Queue : public QueuePosix<T> {
 public:
  using QueuePosix<T>::QueuePosix;
};
}  // namespace ebus::detail::platform
#endif
