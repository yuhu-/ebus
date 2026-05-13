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
  Clock::time_point timestamp;
};

}  // namespace ebus::detail
