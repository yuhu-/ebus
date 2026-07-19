/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/types.hpp>

namespace ebus::detail {

class BusMonitor;

/**
 * Implementation of the eBUS arbitration state machine.
 * Handles "Wire-AND" collision detection, Priority Class checks, and the
 * automatic retry mechanism (Auto-SYN) as defined in Spec 6.2.2.
 */
class Request {
 public:
  explicit Request(BusMonitor* monitor = nullptr);
  void reset();

  // Configuration
  void setLockCounter(uint8_t lock_counter);
  uint8_t getLockCounter() const;
  void setHandlerBusRequestedCallback(Delegate<void()> callback);
  void setExternalBusRequestedCallback(Delegate<void()> callback);
  void setStartBitCallback(Delegate<void()> callback);

  // Working Methods
  bool requestBus(uint8_t address, bool external = false);
  void busRequestCompleted();
  void startBit();
  RequestResult run(uint8_t byte);

  // Status/Telemetry
  bool busAvailable() const;
  RequestState getState() const;
  RequestResult getResult() const;

  // Inline and non-virtual for ESP32 ISR safety (IRAM) and performance
  inline uint8_t busRequestAddress() const { return request_address_; }
  inline bool busRequestPending() const {
    return bus_request_.load(std::memory_order_acquire);
  }
  inline bool busRequestIsExternal() const {
    return external_bus_request_.load(std::memory_order_acquire);
  }

 private:
  BusMonitor* monitor_ = nullptr;

  uint8_t lock_counter_max_ = ebus::RuntimeConfig{}.lock_counter;
  uint8_t lock_counter_ = ebus::RuntimeConfig{}.lock_counter;

  // Counter to detect collisions in observe mode (Spec 6.4 exception)
  uint32_t bytes_since_syn_ = 0;

  uint8_t request_address_ = 0;

  // Indicates whether a bus request is present
  std::atomic<bool> bus_request_ = {false};

  // Indicates whether the bus request is internal or external
  std::atomic<bool> external_bus_request_ = {false};

  Delegate<void()> handler_bus_requested_callback_ = nullptr;
  Delegate<void()> external_bus_requested_callback_ = nullptr;

  // Fired when the physical layer detects a bus anomaly (e.g. noise or framing
  // error) that invalidates the current byte alignment. This allows the Handler
  // to reset its buffers immediately.
  Delegate<void()> start_bit_callback_ = nullptr;

  void observe(uint8_t byte);
  void first(uint8_t byte);
  void retry(uint8_t byte);
  void second(uint8_t byte);

  using StateHandler = void (Request::*)(uint8_t);
  static inline constexpr StateHandler state_requests[] = {
      &Request::observe, &Request::first, &Request::retry, &Request::second};

  static_assert(sizeof(state_requests) / sizeof(state_requests[0]) ==
                    FsmLimits::num_request_states,
                "state_requests table size does not match NUM_REQUEST_STATES");

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observe_syn;

  void transitionTo(RequestState next);
};

}  // namespace ebus::detail
