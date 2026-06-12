/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <ebus/config.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/status.hpp>

#include "core/bus_events.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/mutex.hpp"

namespace ebus::detail {

/**
 * Background worker that processes raw bytes from the Bus queue and feeds
 * them into the Request and Handler state machines. It also manages a
 * registry of byte listeners.
 */
class BusHandler {
 public:
  // Public Types & Constants
  using ByteListener = Delegate<void(const BusEventInfo& info)>;

  // Lifecycle
  BusHandler(Request* request, Handler* handler);
  ~BusHandler();

  // Configuration
  void setWatchdogTimeout(uint32_t timeout_ms);
  uint32_t addByteListener(ByteListener listener);
  void removeByteListener(uint32_t id);

  // Working Methods
  /**
   * @brief Processes a single bus event.
   * This logic is now called directly from the Controller's Reactor loop.
   */
  void processEvent(const BusEvent& bus_event);

  // Status/Telemetry
  BusHandlerStatus getStatus() const;

 private:
  Request* request_;
  Handler* handler_;
  std::chrono::milliseconds watchdog_timeout_ms_{
      ebus::RuntimeConfig{}.bus.watchdog_timeout_ms};

  uint32_t next_listener_id_ = 0;
  mutable platform::Mutex mutex_;
  uint32_t listeners_version_ = 0;
  uint32_t last_cache_version_ = 0xffffffff;

  StaticVector<std::pair<uint32_t, ByteListener>, BusLimits::max_listeners>
      listeners_;
  StaticVector<ByteListener, BusLimits::max_listeners> listeners_cache_;
};

}  // namespace ebus::detail
