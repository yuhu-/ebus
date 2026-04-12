/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace ebus {

/**
 * Configuration that can be updated during runtime without restarting the
 * hardware.
 */
struct RuntimeConfig {
  uint8_t address = 0xff;
  uint16_t window = 4300;
  uint16_t offset = 80;
  uint8_t lock_counter_max = 3;
  uint32_t syn_base_ms = 50;
  uint32_t syn_tolerance_ms = 5;
  bool enable_syn = false;
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
  std::string device = "/dev/ttyUSB0";
  uint32_t baud = 2400;
  bool simulate = false;
};
#endif

/**
 * Global eBUS Controller configuration.
 */
struct EbusConfig {
  RuntimeConfig runtime = {};
  std::chrono::milliseconds client_timeout_ms{500};
  BusConfig bus = {};
};

/**
 * Available client types.
 */
enum class ClientType { read_only, regular, enhanced };

}  // namespace ebus