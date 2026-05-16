/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
// #include <mutex>
#include <vector>

namespace ebus::detail::platform {

/**
 * Base class for all Bus implementations to consolidate common types and
 * listener management.
 */
class BusBase {
 public:
  using ReadListener = std::function<void(const uint8_t& byte)>;
  using WriteListener = std::function<void(const uint8_t& byte)>;
  using SynListener = std::function<void()>;

  virtual ~BusBase() = default;

 protected:
  // Common storage for listeners.
  // Derived classes handle synchronization (Mutex vs Spinlock).
  std::vector<ReadListener> read_listeners_;
  std::vector<WriteListener> write_listeners_;
  std::vector<SynListener> syn_listeners_;
};

}  // namespace ebus::detail::platform
