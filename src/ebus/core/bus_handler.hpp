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
 * The BusHandler (along with Request and Handler) is a passive FSM
 * (Finite State Machine). It contains state variables, transition tables, and
 * protocol logic, but it does not have an execution loop or run-time thread
 * context of its own.
 */
class BusHandler {
 public:
  // Lifecycle
  BusHandler(Request* request, Handler* handler);
  ~BusHandler();

  // Configuration
  void setWatchdogTimeout(uint32_t timeout_ms);
  void setClientManagerBusEventInfoCallback(
      Delegate<void(const BusEventInfo& info)> callback);
  void setControllerBusEventInfoCallback(
      Delegate<void(const BusEventInfo& info)> callback);

  // Working Methods
  /**
   * @brief Processes a single bus event.
   * This logic is now called directly from the Controller's Reactor loop.
   */
  void onBusEvent(const BusEvent& bus_event);

  // Status/Telemetry
  BusHandlerStatus fetchStatus() const;

 private:
  Request* request_;
  Handler* handler_;
  std::chrono::milliseconds watchdog_timeout_ms_{
      ebus::RuntimeConfig{}.bus.watchdog_timeout_ms};

  Delegate<void(const BusEventInfo& info)> client_manager_callback_ = nullptr;
  Delegate<void(const BusEventInfo& info)> controller_callback_ = nullptr;

  mutable platform::Mutex mutex_;
};

}  // namespace ebus::detail
