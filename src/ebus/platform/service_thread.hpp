/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>
#include <memory>
#include <string>

namespace ebus::detail {

/**
 * Platform-independent abstraction for a background worker.
 * Wraps std::thread on POSIX and FreeRTOS tasks on ESP32.
 */
class ServiceThread {
 public:
  ServiceThread(std::string name, std::function<void()> func,
                uint32_t stack_size = OrchestrationLimits::stack_size,
                uint8_t priority = OrchestrationLimits::priority_low,
                int core = -1);
  ~ServiceThread();

  void start();
  void join();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ebus::detail
