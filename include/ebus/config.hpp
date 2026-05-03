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
  bool system_inquiry = false;
  bool system_response = true;

  struct Bus {
    uint16_t window_us = 4300;  // us
    uint16_t offset_us = 80;    // us
    uint32_t watchdog_timeout_ms = 250;

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
    uint32_t session_timeout_ms = 500;
    uint32_t transmit_timeout_ms = 250;
    size_t outbound_buffer_size = 4096;
  } network;

  struct Scanner {
    bool scan_on_startup = true;
    uint32_t initial_delay_s = 10;
    uint32_t startup_interval_s = 60;
    uint8_t max_startup_scans = 5;
  } scanner;

  struct Scheduler {
    uint8_t max_send_attempts = 3;
    uint32_t base_backoff_ms = 100;
    uint32_t fsm_timeout_ms = 1000;
    uint32_t total_timeout_ms = 2000;
  } scheduler;

  /**
   * Resets all fields to their default hardcoded values.
   */
  void reset() { *this = RuntimeConfig{}; }
};

/**
 * Platform-dependent bus configuration.
 */
#if defined(ESP_PLATFORM)
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

/**
 * Serializes the full EbusConfig to a JSON string.
 */
std::string toJson(const EbusConfig& config);

}  // namespace ebus