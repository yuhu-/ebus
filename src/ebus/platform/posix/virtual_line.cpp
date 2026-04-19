/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform/posix/virtual_line.hpp"

#include <algorithm>

// Initialize static members
std::mutex ebus::VirtualLine::registry_mutex_;
std::vector<ebus::VirtualLine*> ebus::VirtualLine::instances_;

ebus::VirtualLine::VirtualLine() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  instances_.push_back(this);
}

ebus::VirtualLine::~VirtualLine() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  instances_.erase(std::remove(instances_.begin(), instances_.end(), this),
                   instances_.end());
}

void ebus::VirtualLine::write(uint8_t byte) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  // Simulates the physical wire: everyone connected sees the signal
  for (auto* instance : instances_) {
    instance->rx_queue_.tryPush(byte);
  }
}

bool ebus::VirtualLine::read(uint8_t& byte, int timeout_ms) {
  return rx_queue_.pop(byte, timeout_ms);
}
