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
  uint8_t address = defaults::address;

  struct Arbitration {
    uint8_t lock_counter_max = defaults::Arbitration::lock_counter;
  } arbitration;

  struct Bus {
    struct Timing {
      uint16_t window = defaults::Bus::window;
      uint16_t offset = defaults::Bus::offset;
    } timing;

    struct Syn {
      bool enabled = false;
      uint32_t base_ms = defaults::Bus::Syn::base_ms;
      uint32_t tolerance_ms = defaults::Bus::Syn::tolerance_ms;
    } syn;
  } bus;

  struct Scheduler {
    int max_send_attempts = defaults::Scheduler::max_send_attempts;
    struct Timing {
      std::chrono::milliseconds base_backoff{
          defaults::Scheduler::base_backoff_ms};
      std::chrono::milliseconds fsm_timeout{
          defaults::Scheduler::fsm_timeout_ms};
      std::chrono::milliseconds total_timeout{
          defaults::Scheduler::total_timeout_ms};
    } timing;
  } scheduler;

  struct Network {
    struct Timing {
      std::chrono::milliseconds client_timeout{
          defaults::Network::client_timeout_ms};
      std::chrono::milliseconds watchdog_timeout{
          defaults::Network::watchdog_timeout_ms};
    } timing;
    size_t outbound_buffer_size = defaults::Network::outbound_buffer_size;
  } network;

  struct Logging {
    LogLevel level = LogLevel::error;
    size_t log_size = defaults::Logging::log_size;
  } logging;
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
  std::string device = defaults::Bus::device_path;
  uint32_t baud = defaults::Bus::baud_rate;
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