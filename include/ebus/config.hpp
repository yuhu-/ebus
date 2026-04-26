/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "ebus/types.hpp"

namespace ebus {

/**
 * Configuration that can be updated during runtime without restarting the
 * hardware.
 */
struct RuntimeConfig {
  uint8_t address = 0xff;
  uint8_t lock_counter = 3;  // max 25

  struct Bus {
    uint16_t window_us = 4300;  // us
    uint16_t offset_us = 80;    // us

    struct Syn {
      bool enabled = false;
      uint32_t base_ms = 50;
      uint32_t tolerance_ms = 5;
    } syn;
  } bus;

  struct Logging {
    LogLevel level = LogLevel::error;
    size_t log_size = 10;
  } logging;

  struct Network {
    uint32_t client_timeout_ms = 1000;
    uint32_t watchdog_timeout_ms = 5000;
    size_t outbound_buffer_size = 4096;
  } network;

  struct Scheduler {
    int max_send_attempts = 3;
    uint32_t base_backoff_ms = 100;
    uint32_t fsm_timeout_ms = 2000;
    uint32_t total_timeout_ms = 4000;
  } scheduler;

  /**
   * Resets all fields to their default hardcoded values.
   */
  void reset() { *this = RuntimeConfig{}; }
};

/**
 * Platform-dependent bus configuration.
 */
#if defined(ESP32)
struct BusConfig {
  uint8_t uart_port;
  uint8_t rx_pin;
  uint8_t tx_pin;
  uint8_t timer_group;
  uint8_t timer_idx;
};
#elif defined(POSIX)
struct BusConfig {
  std::string device = "/dev/null";
  bool simulate = false;
};
#endif

/**
 * Global eBUS Controller configuration.
 */
struct EbusConfig {
  RuntimeConfig runtime = {};
  BusConfig bus = {};
};

}  // namespace ebus