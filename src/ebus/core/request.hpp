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

static const char* getRequestStateText(RequestState state) {
  const char* values[] = {"observe", "first", "retry", "second"};
  return values[static_cast<int>(state)];
}

enum class RequestResult {
  observe_syn,
  observe_data,
  first_syn,
  first_won,
  first_retry,
  first_lost,
  first_error,
  retry_syn,
  retry_error,
  second_won,
  second_lost,
  second_error
};

static const char* getRequestResultText(RequestResult result) {
  const char* values[] = {"observe_syn", "observe_data", "first_syn",
                          "first_won",   "first_retry",  "first_lost",
                          "first_error", "retry_syn",    "retry_error",
                          "second_won",  "second_lost",  "second_error"};
  return values[static_cast<int>(result)];
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

#define EBUS_REQUEST_COUNTER_LIST \
  X(first_syn)                    \
  X(first_won)                    \
  X(first_retry)                  \
  X(first_lost)                   \
  X(first_error)                  \
  X(retry_syn)                    \
  X(retry_error)                  \
  X(second_won)                   \
  X(second_lost)                  \
  X(second_error)

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
  std::map<std::string, MetricValues> getMetrics() const;

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

  std::array<void (Request::*)(uint8_t), NUM_REQUEST_STATES> state_requests_ =
      {};

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observe_syn;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_REQUEST_COUNTER_LIST
#undef X
  };

  Counter counter_;

  void observe(uint8_t byte);
  void first(uint8_t byte);
  void retry(uint8_t byte);
  void second(uint8_t byte);
};

}  // namespace ebus
