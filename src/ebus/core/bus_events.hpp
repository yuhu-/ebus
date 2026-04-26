/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>

#include "ebus/types.hpp"

namespace ebus::detail {

/**
 * Represents a single event on the bus, including the byte value, whether it
 * was associated with a bus request or start bit, and the timestamp of when it
 * was captured.
 */
struct BusEvent {
  uint8_t byte;
  bool bus_request{false};
  bool start_bit{false};
  std::chrono::steady_clock::time_point timestamp;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 */
struct BusEventContext {
  uint8_t byte;
  RequestState state;
  RequestResult result;
  uint8_t lock_counter;
  std::chrono::steady_clock::time_point timestamp;
};

}  // namespace ebus::detail
