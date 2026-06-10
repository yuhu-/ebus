/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>

#include "core/bus_events.hpp"
#include "platform/delegate.hpp"
#include "platform/mutex.hpp"
#include "utils/static_vector.hpp"

namespace ebus::detail::platform {

/**
 * Base class for all Bus implementations to consolidate common types and
 * listener management.
 */
class BusBase {
 public:
  // Public Types & Constants
  using ReadListener = Delegate<void(const uint8_t& byte)>;
  using WriteListener = Delegate<void(const uint8_t& byte)>;
  using SynListener = Delegate<void()>;
  using BusEventListener = Delegate<void(const BusEvent& event)>;

  // Lifecycle
  virtual ~BusBase() = default;

  // Working Methods
  void addReadListener(ReadListener listener) {
    platform::LockGuard<platform::Mutex> lock(listeners_mutex_);
    read_listeners_.push_back(std::move(listener));
  }

  void addWriteListener(WriteListener listener) {
    platform::LockGuard<platform::Mutex> lock(listeners_mutex_);
    write_listeners_.push_back(std::move(listener));
  }

  void addSynListener(SynListener listener) {
    platform::LockGuard<platform::Mutex> lock(listeners_mutex_);
    syn_listeners_.push_back(std::move(listener));
  }

  void addBusEventListener(BusEventListener listener) {
    platform::LockGuard<platform::Mutex> lock(listeners_mutex_);
    bus_event_listeners_.push_back(std::move(listener));
  }

  // Status/Telemetry
 protected:
  mutable platform::Mutex listeners_mutex_;

  const StaticVector<ReadListener, BusLimits::max_listeners>& getReadListeners()
      const {
    return read_listeners_;
  }

  const StaticVector<WriteListener, BusLimits::max_listeners>&
  getWriteListeners() const {
    return write_listeners_;
  }

  const StaticVector<SynListener, BusLimits::max_listeners>& getSynListeners()
      const {
    return syn_listeners_;
  }

  const StaticVector<BusEventListener, BusLimits::max_listeners>&
  getBusEventListeners() const {
    return bus_event_listeners_;
  }

  /**
   * @brief Safely invokes listeners by copying them under a lock and executing
   * outside. Handles any Lockable type and variadic arguments for the listener
   * signature.
   */
  template <typename Lockable, typename ListenerType, typename... Args>
  void lockAndInvoke(
      Lockable& m,
      const StaticVector<ListenerType, BusLimits::max_listeners>& listeners,
      Args&&... args) {
    ListenerType cache[BusLimits::max_listeners];
    size_t n = 0;
    {
      platform::LockGuard<Lockable> lock(m);
      n = copyToCache(listeners, cache);
    }
    for (size_t i = 0; i < n; ++i) cache[i](std::forward<Args>(args)...);
  }

  /**
   * @brief Low-level helper to copy listeners to a stack array.
   * Useful for ESP32 critical sections where platform::LockGuard is not
   * applicable.
   */
  template <typename ListenerType>
  size_t copyToCache(
      const StaticVector<ListenerType, BusLimits::max_listeners>& src,
      ListenerType* dst) {
    size_t n = 0;
    for (const auto& l : src) dst[n++] = l;
    return n;
  }

 private:
  /**
   * Common storage for listeners using StaticVector to avoid heap
   * fragmentation. Capacity is enforced by BusLimits::max_listeners.
   */
  StaticVector<ReadListener, BusLimits::max_listeners> read_listeners_;
  StaticVector<WriteListener, BusLimits::max_listeners> write_listeners_;
  StaticVector<SynListener, BusLimits::max_listeners> syn_listeners_;
  StaticVector<BusEventListener, BusLimits::max_listeners> bus_event_listeners_;
};

}  // namespace ebus::detail::platform
