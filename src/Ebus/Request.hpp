/*
 * Copyright (C) 2025-2026 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

// Implementation of eBUS arbitration logic for internal (handler) and external
// bus access.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>

#include "TimingStats.hpp"

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

using BusRequestedCallback = std::function<void()>;
using StartBitCallback = std::function<void()>;

#define EBUS_REQUEST_COUNTER_LIST \
  X(requestsFirstSyn)             \
  X(requestsFirstWon)             \
  X(requestsFirstRetry)           \
  X(requestsFirstLost)            \
  X(requestsFirstError)           \
  X(requestsRetrySyn)             \
  X(requestsRetryError)           \
  X(requestsSecondWon)            \
  X(requestsSecondLost)           \
  X(requestsSecondError)

class Request {
 public:
  // measurement
  struct Counter {
#define X(name) uint32_t name = 0;
    EBUS_REQUEST_COUNTER_LIST
#undef X
  };

  explicit Request();

  void setMaxLockCounter(const uint8_t& maxCounter);
  uint8_t getLockCounter() const;

  bool busAvailable() const;

  // Request the bus from handler or external
  bool requestBus(const uint8_t& address, const bool& external = false);

  void setHandlerBusRequestedCallback(BusRequestedCallback callback);
  void setExternalBusRequestedCallback(BusRequestedCallback callback);

  uint8_t busRequestAddress() const;

  bool busRequestPending() const;
  void busRequestCompleted();

  void startBit();
  void setStartBitCallback(StartBitCallback callback);

  RequestState getState() const;
  RequestResult getResult() const;

  void reset();

  RequestResult run(const uint8_t& byte);

  void resetCounter();
  const Counter& getCounter() const;

 private:
  uint8_t maxLockCounter_ = DEFAULT_LOCK_COUNTER;
  uint8_t lockCounter_ = DEFAULT_LOCK_COUNTER;

  uint8_t requestAddress_ = 0;

  // Indicates whether a bus request is present
  bool busRequest_ = false;

  // Indicates whether the bus request is internal or external
  bool externalBusRequest_ = false;

  BusRequestedCallback handlerBusRequestedCallback_ = nullptr;
  BusRequestedCallback externalBusRequestedCallback_ = nullptr;

  StartBitCallback startBitCallback_ = nullptr;

  std::array<void (Request::*)(const uint8_t&), NUM_REQUEST_STATES>
      stateRequests_ = {};

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observeSyn;

  // measurement
  Counter counter_;

  void observe(const uint8_t& byte);
  void first(const uint8_t& byte);
  void retry(const uint8_t& byte);
  void second(const uint8_t& byte);

  bool checkPriorityClassSubAddress(const uint8_t& byte);
};

}  // namespace ebus
