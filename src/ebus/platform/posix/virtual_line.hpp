/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "platform/queue.hpp"

namespace ebus {

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
  bool read(uint8_t& byte, int timeoutMs = 10);

 private:
  // Internal queue for bytes arriving from the "wire"
  Queue<uint8_t> rxQueue_{256};

  // Static registry members to bridge multiple instances
  static std::mutex registryMutex_;
  static std::vector<VirtualLine*> instances_;
};

}  // namespace ebus
