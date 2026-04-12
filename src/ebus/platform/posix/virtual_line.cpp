/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform/posix/virtual_line.hpp"

#include <algorithm>

namespace ebus {

// Initialize static members
std::mutex VirtualLine::registryMutex_;
std::vector<VirtualLine*> VirtualLine::instances_;

VirtualLine::VirtualLine() {
  std::lock_guard<std::mutex> lock(registryMutex_);
  instances_.push_back(this);
}

VirtualLine::~VirtualLine() {
  std::lock_guard<std::mutex> lock(registryMutex_);
  instances_.erase(std::remove(instances_.begin(), instances_.end(), this),
                   instances_.end());
}

void VirtualLine::write(uint8_t byte) {
  std::lock_guard<std::mutex> lock(registryMutex_);
  // Simulates the physical wire: everyone connected sees the signal
  for (auto* instance : instances_) {
    instance->rxQueue_.try_push(byte);
  }
}

bool VirtualLine::read(uint8_t& byte, int timeoutMs) {
  return rxQueue_.pop(byte, timeoutMs);
}

}  // namespace ebus
