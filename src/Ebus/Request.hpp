/*
 * Copyright (C) 2025 Roland Jax
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

#include "Statistic.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_LOCK_COUNTER = 3;
constexpr uint8_t MAX_LOCK_COUNTER = 25;

constexpr size_t NUM_REQUEST_STATES = 4;

enum class RequestState { observe, first, retry, second };

static const char *getRequestStateText(RequestState state) {
  const char *values[] = {"observe", "first", "retry", "second"};
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

static const char *getRequestResultText(RequestResult result) {
  const char *values[] = {"observeSyn", "observeData", "firstSyn",
                          "firstWon",   "firstRetry",  "firstLost",
                          "firstError", "retrySyn",    "retryError",
                          "secondWon",  "secondLost",  "secondError"};
  return values[static_cast<int>(result)];
}

using BusRequestedCallback = std::function<void()>;
using StartBitCallback = std::function<void()>;

#define EBUS_REQUEST_COUNTER_LIST \
  X(requestsStartBit)             \
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

#define EBUS_REQUEST_TIMING_LIST \
  X(busIsrDelay)                 \
  X(busIsrWindow)

class Request {
 public:
  // measurement
  struct Counter {
#define X(name) uint32_t name = 0;
    EBUS_REQUEST_COUNTER_LIST
#undef X
  };

  struct Timing {
#define X(name)             \
  double name##Last = 0;    \
  uint64_t name##Count = 0; \
  double name##Mean = 0;    \
  double name##StdDev = 0;
    EBUS_REQUEST_TIMING_LIST
#undef X
  };

  explicit Request();

  void setMaxLockCounter(const uint8_t &maxCounter);
  const uint8_t getLockCounter() const;

  // Request the bus from handler or external
  bool requestBus(const uint8_t &address, const bool &external = false);

  void setHandlerBusRequestedCallback(BusRequestedCallback callback);
  void setExternalBusRequestedCallback(BusRequestedCallback callback);

  bool busRequestPending() const;
  void busRequestCompleted();

  void startBit();
  void setStartBitCallback(StartBitCallback callback);

  RequestState getState() const;
  RequestResult getResult() const;

  void reset();

  RequestResult run(const uint8_t &byte);

  void microsLastDelay(const int64_t &delay);
  void microsLastWindow(const int64_t &window);

  void resetCounter();
  const Counter &getCounter() const;

  void resetTiming();
  const Timing &getTiming();

 private:
  uint8_t sourceAddress = 0;

  uint8_t maxLockCounter = DEFAULT_LOCK_COUNTER;
  uint8_t lockCounter = DEFAULT_LOCK_COUNTER;

  // Indicates whether a bus request is present
  bool busRequest = false;

  // Indicates whether the bus request is internal or external
  bool externalBusRequest = false;

  BusRequestedCallback busRequestedCallback = nullptr;
  BusRequestedCallback externalBusRequestedCallback = nullptr;

  StartBitCallback startBitCallback = nullptr;

  std::array<void (Request::*)(const uint8_t &), NUM_REQUEST_STATES>
      stateRequests;

  RequestState state = RequestState::observe;
  RequestResult result = RequestResult::observeSyn;

  // measurement
  Counter counter;
  Timing timing;

  TimingStats busIsrDelay;
  TimingStats busIsrWindow;

  void observe(const uint8_t &byte);
  void first(const uint8_t &byte);
  void retry(const uint8_t &byte);
  void second(const uint8_t &byte);

  bool checkPriorityClassSubAddress(const uint8_t &byte);
};

}  // namespace ebus
