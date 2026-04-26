/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(POSIX)
#include <cstdint>
#include <ebus/detail/protocol_limits.hpp>
#include <mutex>
#include <vector>

#include "platform/queue.hpp"

namespace ebus::detail {

/**
 * VirtualLine simulates the physical eBUS wire.
 * It uses a static registry to ensure that a byte written by one instance
 * is received by all other instances connected to the "virtual bus".
 */
class VirtualLine {
 public:
  VirtualLine();
  ~VirtualLine();

  // Broadcasts a byte to all instances (including self for echo)
  void write(uint8_t byte);

  // Reads a byte from this specific instance's receive buffer
  bool read(uint8_t& byte,
            int timeout_ms = BusLimits::Posix::virtual_read_timeout_ms);

 private:
  // Internal queue for bytes arriving from the "wire"
  Queue<uint8_t> rx_queue_{BusLimits::queue_size};

  // Static registry members to bridge multiple instances
  static std::mutex registry_mutex_;
  static std::vector<VirtualLine*> instances_;
};

}  // namespace ebus::detail

#endif
