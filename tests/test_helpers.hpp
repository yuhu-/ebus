/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <app/client.hpp>
#include <app/client_manager.hpp>
#include <app/scheduler.hpp>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/service_thread.hpp"
#include "platform/socket.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

/**
 * Robust read helper to handle partial TCP/Socket reads.
 */
inline bool readExact(int fd, uint8_t* buffer, size_t length) {
  size_t total = 0;
  while (total < length) {
    ssize_t n = read(fd, buffer + total, length - total);
    if (n <= 0) return false;
    total += n;
  }
  return true;
}

/**
 * Helper to wait for a condition to become true without hardcoded sleeps.
 */
template <typename Predicate>
inline bool waitCondition(Predicate&& pred, int timeout_ms = 1000) {
  const auto start = ebus::Clock::now();
  while (ebus::Clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
    if (pred()) {
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }
    platform::sleepMilli(5);
  }
  return pred();
}

/**
 * @brief Blocks until the predicate is true, pumping the reactor in a loop.
 */
template <typename Predicate>
bool waitFor(Predicate&& pred,
             std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  auto start = Clock::now();
  while (!pred() && (Clock::now() - start < timeout)) {
    platform::sleepMilli(1);
  }
  return pred();
}

/**
 * @brief Reads from a socket while pumping the reactor to avoid deadlocks.
 * Replaces blocking readExact for network bridge tests.
 */
bool readFromSocket(
    int fd, uint8_t* buf, size_t len,
    std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
  size_t received = 0;
  return waitFor(
      [&]() {
        ssize_t n = platform::recv(fd, buf + received, len - received,
                                   platform::Flags::dont_wait);
        if (n > 0) received += static_cast<size_t>(n);
        return received == len;
      },
      timeout);
}

}  // namespace ebus::detail
