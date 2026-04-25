/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace ebus {

// --- Physical Layer ---
struct Physical {
  static constexpr uint32_t baud_rate = 2400;
  static constexpr double bit_time_us = 1000000.0 / 2400.0;  // ~416.67 us
  static constexpr double byte_center_bits = 9.5;  // midpoint of the stop bit
};

// --- Component Layer ---
struct FSM {
  static constexpr std::size_t num_handler_states = 15;
  static constexpr size_t num_request_states = 4;
};

// --- Defaults ---
namespace defaults {
constexpr uint8_t address = 0xff;

struct Orchestration {
  static constexpr size_t stack_size = 4096;
  static constexpr uint8_t priority_high = 5;
  static constexpr uint8_t priority_med = 3;
  static constexpr uint8_t priority_low = 1;
};

struct Arbitration {
  static constexpr uint8_t lock_counter = 3;
  static constexpr uint8_t max_lock_counter = 25;
};

struct Bus {
  static constexpr uint16_t window = 4300;  // us
  static constexpr uint16_t min_window = 4000;
  static constexpr uint16_t max_window = 5000;

  static constexpr uint16_t offset = 80;  // us
  static constexpr uint16_t max_offset = 500;

  static constexpr size_t queue_size = 256;
  static constexpr size_t event_queue_capacity = 16;
  static constexpr size_t max_listeners = 16;

  struct Syn {
    static constexpr uint32_t base_ms = 50;
    static constexpr uint32_t tolerance_ms = 5;
    static constexpr uint32_t address_factor_ms = 10;
    static constexpr uint32_t postpone_ms = 2;
    static constexpr uint32_t carrier_sense_ms = 5;
    // TODO use this in FreeRTOS to ?
    static constexpr uint32_t serialization_delay_ms = 4;
  };

  struct Posix {
    static constexpr const char* device_path = "/dev/null";
    static constexpr uint32_t arbitration_delay_us = 200;
    static constexpr uint32_t virtual_read_timeout_ms = 10;
  };
};

struct Logging {
  static constexpr size_t log_size = 10;
  static constexpr size_t history_size = 60;
};

struct Network {
  static constexpr uint32_t client_timeout_ms = 1000;
  static constexpr uint32_t watchdog_timeout_ms = 5000;
  static constexpr size_t outbound_buffer_size = 4096;
  static constexpr uint32_t transmit_timeout_ms = 500;
  static constexpr uint32_t wake_interval_ms = 20;
};

struct PollManager {
  static constexpr uint32_t default_interval_ms = 5000;
};

struct Scheduler {
  static constexpr size_t queue_reserve = 32;
  static constexpr size_t scan_threshold = 5;
  static constexpr uint32_t controller_tick_ms = 20;

  static constexpr int max_send_attempts = 3;
  static constexpr uint32_t base_backoff_ms = 100;
  static constexpr uint32_t fsm_timeout_ms = 2000;
  static constexpr uint32_t total_timeout_ms = 4000;
};

struct Scanner {
  static constexpr uint32_t initial_delay_s = 10;
  static constexpr uint32_t startup_interval_s = 60;
  static constexpr uint8_t max_startup_scans = 5;
  static constexpr uint8_t scan_priority = 5;
};

struct Sequence {
  // Default SBO size
  static constexpr size_t default_capacity = 64;
  // Safe upper bound for MS telegrams
  static constexpr uint8_t max_telegram_bytes = 48;
  // Maximum data bytes (Spec 5.6)
  static constexpr uint8_t max_data_bytes = 16;
};

static_assert(defaults::Bus::offset < defaults::Bus::min_window,
              "The default offset must be smaller than the minimum arbitration "
              "window to prevent timing underflow.");

}  // namespace defaults
}  // namespace ebus