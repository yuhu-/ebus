/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>

#include "utils/static_vector.hpp"

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
  auto& getReadListeners() { return read_listeners_; }
  auto& getWriteListeners() { return write_listeners_; }
  auto& getSynListeners() { return syn_listeners_; }

 private:
  /**
   * Common storage for listeners using StaticVector to avoid heap
   * fragmentation. Capacity is enforced by BusLimits::max_listeners.
   */
  StaticVector<ReadListener, BusLimits::max_listeners> read_listeners_;
  StaticVector<WriteListener, BusLimits::max_listeners> write_listeners_;
  StaticVector<SynListener, BusLimits::max_listeners> syn_listeners_;
};

}  // namespace ebus::detail::platform
