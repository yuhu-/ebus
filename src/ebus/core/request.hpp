/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ebus/definitions.hpp>
#include <ebus/metrics.hpp>
#include <functional>
#include <map>
#include <string>

#include "utils/timing_stats.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_LOCK_COUNTER = 3;
constexpr uint8_t MAX_LOCK_COUNTER = 25;

constexpr size_t NUM_REQUEST_STATES = 4;

enum class RequestState { observe, first, retry, second };

constexpr const char* toString(RequestState state) {
  switch (state) {
    case RequestState::observe:
      return "observe";
    case RequestState::first:
      return "first";
    case RequestState::retry:
      return "retry";
    case RequestState::second:
      return "second";
    default:
      return "unknown state";
  }
}

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 */
struct BusEventContext {
  uint8_t byte;
  RequestState state;
  RequestResult result;
  uint8_t lock_counter;
  std::chrono::steady_clock::time_point timestamp;
};

using BusRequestedCallback = std::function<void()>;
using StartBitCallback = std::function<void()>;

/**
 * Implementation of the eBUS arbitration state machine.
 * Handles "Wire-AND" collision detection, Priority Class checks, and the
 * automatic retry mechanism (Auto-SYN) as defined in Spec 6.2.2.
 */
class Request {
 public:
  explicit Request();

  void setMaxLockCounter(uint8_t max_counter);
  uint8_t getLockCounter() const;

  bool busAvailable() const;

  // Request the bus from handler or external
  bool requestBus(uint8_t address, bool external = false);

  void setHandlerBusRequestedCallback(BusRequestedCallback callback);
  void setExternalBusRequestedCallback(BusRequestedCallback callback);

  // Inline and non-virtual for ESP32 ISR safety (IRAM) and performance
  inline uint8_t busRequestAddress() const { return request_address_; }

  inline bool busRequestPending() const {
    return bus_request_.load(std::memory_order_acquire);
  }

  void busRequestCompleted();

  void startBit();
  void setStartBitCallback(StartBitCallback callback);

  RequestState getState() const;
  RequestResult getResult() const;

  void reset();

  RequestResult run(uint8_t byte);

  void resetMetrics();
  metrics::RequestMetrics getMetrics() const;

 private:
  uint8_t max_lock_counter_ = DEFAULT_LOCK_COUNTER;
  uint8_t lock_counter_ = DEFAULT_LOCK_COUNTER;

  uint8_t request_address_ = 0;

  // Indicates whether a bus request is present
  std::atomic<bool> bus_request_ = {false};

  // Indicates whether the bus request is internal or external
  bool external_bus_request_ = false;

  BusRequestedCallback handler_bus_requested_callback_ = nullptr;
  BusRequestedCallback external_bus_requested_callback_ = nullptr;

  StartBitCallback start_bit_callback_ = nullptr;

  // Internal storage for detailed counters
  ebus::metrics::RequestMetrics metrics_storage_;

  void observe(uint8_t byte);
  void first(uint8_t byte);
  void retry(uint8_t byte);
  void second(uint8_t byte);

  using StateHandler = void (Request::*)(uint8_t);
  static inline constexpr StateHandler kStateRequests[] = {
      &Request::observe, &Request::first, &Request::retry, &Request::second};

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observe_syn;
};

}  // namespace ebus
