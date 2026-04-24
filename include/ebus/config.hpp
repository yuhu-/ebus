/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "ebus/definitions.hpp"

namespace ebus {

/**
 * Configuration that can be updated during runtime without restarting the
 * hardware.
 */
struct RuntimeConfig {
  uint8_t address = default_address;
  uint16_t window = default_window;
  uint16_t offset = default_offset;
  uint8_t lock_counter_max = default_lock_counter;
  LogLevel log_level = LogLevel::error;
  size_t error_log_size = default_error_log_size;
  uint32_t syn_base_ms = default_syn_base_ms;
  uint32_t syn_tolerance_ms = default_syn_tolerance_ms;
  bool enable_syn = false;
  int max_send_attempts = default_max_send_attempts;
  std::chrono::milliseconds base_backoff_ms{default_base_backoff_ms};
  std::chrono::milliseconds client_timeout_ms{default_client_timeout_ms};
  std::chrono::milliseconds fsm_timeout_ms{default_fsm_timeout_ms};
  std::chrono::milliseconds watchdog_timeout_ms{default_watchdog_timeout_ms};
  std::chrono::milliseconds scheduler_total_timeout_ms{
      default_scheduler_total_timeout_ms};
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
  std::string device = default_device_path;
  uint32_t baud = default_baud_rate;
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