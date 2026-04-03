/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "Utils/TimingStats.hpp"

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
  observeSyn,
  observeData,
  firstSyn,
  firstWon,
  firstRetry,
  firstLost,
  firstError,
  retrySyn,
  retryError,
  secondWon,
  secondLost,
  secondError
};

static const char* getRequestResultText(RequestResult result) {
  const char* values[] = {"observeSyn", "observeData", "firstSyn",
                          "firstWon",   "firstRetry",  "firstLost",
                          "firstError", "retrySyn",    "retryError",
                          "secondWon",  "secondLost",  "secondError"};
  return values[static_cast<int>(result)];
}

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 */
struct BusEventContext {
  uint8_t byte;
  RequestState state;
  RequestResult result;
  uint8_t lockCounter;
  std::chrono::steady_clock::time_point timestamp;
};

using BusRequestedCallback = std::function<void()>;
using StartBitCallback = std::function<void()>;

#define EBUS_REQUEST_COUNTER_LIST \
  X(firstSyn)                     \
  X(firstWon)                     \
  X(firstRetry)                   \
  X(firstLost)                    \
  X(firstError)                   \
  X(retrySyn)                     \
  X(retryError)                   \
  X(secondWon)                    \
  X(secondLost)                   \
  X(secondError)

/**
 * Implementation of the eBUS arbitration state machine.
 * Handles "Wire-AND" collision detection, Priority Class checks, and the
 * automatic retry mechanism (Auto-SYN) as defined in Spec 6.2.2.
 */
class Request {
 public:
  explicit Request();

  void setMaxLockCounter(const uint8_t& maxCounter);
  uint8_t getLockCounter() const;

  bool busAvailable() const;

  // Request the bus from handler or external
  bool requestBus(const uint8_t& address, const bool& external = false);

  void setHandlerBusRequestedCallback(BusRequestedCallback callback);
  void setExternalBusRequestedCallback(BusRequestedCallback callback);

  // Inline and non-virtual for ESP32 ISR safety (IRAM) and performance
  inline uint8_t busRequestAddress() const { return requestAddress_; }

  inline bool busRequestPending() const {
    return busRequest_.load(std::memory_order_acquire);
  }

  void busRequestCompleted();

  void startBit();
  void setStartBitCallback(StartBitCallback callback);

  RequestState getState() const;
  RequestResult getResult() const;

  void reset();
  
  uint32_t getVersion() const { return version_.load(std::memory_order_acquire); }

  RequestResult run(const uint8_t& byte);

  void resetMetrics();
  std::map<std::string, MetricValues> getMetrics() const;

 private:
  uint8_t maxLockCounter_ = DEFAULT_LOCK_COUNTER;
  uint8_t lockCounter_ = DEFAULT_LOCK_COUNTER;

  uint8_t requestAddress_ = 0;

  // Indicates whether a bus request is present
  std::atomic<bool> busRequest_ = {false};

  // Indicates whether the bus request is internal or external
  bool externalBusRequest_ = false;

  BusRequestedCallback handlerBusRequestedCallback_ = nullptr;
  BusRequestedCallback externalBusRequestedCallback_ = nullptr;

  StartBitCallback startBitCallback_ = nullptr;

  std::array<void (Request::*)(const uint8_t&), NUM_REQUEST_STATES>
      stateRequests_ = {};

  std::atomic<uint32_t> version_{0};

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observeSyn;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_REQUEST_COUNTER_LIST
#undef X
  };

  Counter counter_;

  void observe(const uint8_t& byte);
  void first(const uint8_t& byte);
  void retry(const uint8_t& byte);
  void second(const uint8_t& byte);
};

}  // namespace ebus
