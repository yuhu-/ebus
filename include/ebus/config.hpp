/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "ebus/types.hpp"

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

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
    bool syn_gen = false;
  } bus;

  struct Diagnostics {
    LogLevel level = LogLevel::error;
    size_t log_size = 5;
  } diagnostics;

  struct Network {
    uint32_t session_timeout_ms = 500;
    uint32_t transmit_timeout_ms = 250;
    size_t outbound_buffer_size = 4096;
  } network;

  struct Device {
    bool scan_on_startup = false;
    uint32_t initial_delay_s = 10;
    uint32_t startup_interval_s = 60;
    uint8_t max_startup_scans = 5;
  } device;

  struct Scheduler {
    uint8_t max_send_attempts = 3;
    uint32_t base_backoff_ms = 100;
    uint32_t fsm_timeout_ms = 1000;
    uint32_t total_timeout_ms = 2000;
    size_t max_items = 32;
  } scheduler;

  struct Poll {
    size_t max_items = 128;
  } poll;

  void reset();

  void toJson(detail::JsonWriter& writer) const;

  /**
   * @brief Deserializes a JSON string into a RuntimeConfig object.
   * @param json The JSON string to parse.
   * @return A populated RuntimeConfig object. Defaults are used for missing
   * keys.
   */
  static RuntimeConfig fromJson(std::string_view json);

  /**
   * @brief Merges a partial JSON string into the current configuration.
   * Only keys present in the JSON are updated; others remain unchanged.
   * Unknown keys are safely ignored.
   * @return true if the JSON was partially or fully parsed, false on error.
   */
  bool mergeFromJson(std::string_view json);

  /**
   * @brief Performs a basic structural validation of a JSON string.
   * Checks for non-empty string, starts/ends with braces, and balanced
   * braces/brackets.
   * @param json The JSON string to validate.
   * @return true if the string appears to be valid JSON, false otherwise.
   */
  static bool isValidJson(std::string_view json);
};

/**
 * Platform-dependent bus configuration.
 */
struct BusConfig {
#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
  uint8_t uart_port;
  uint8_t rx_pin;
  uint8_t tx_pin;
  uint8_t timer_group;
  uint8_t timer_idx;
#elif defined(POSIX) && !EBUS_SIMULATION
  std::string device = "/dev/null";
#endif

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Global eBUS Controller configuration.
 */
struct EbusConfig {
  RuntimeConfig runtime = {};
  BusConfig bus = {};

  void toJson(detail::JsonWriter& writer) const;
};

}  // namespace ebus