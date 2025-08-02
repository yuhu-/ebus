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

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Statistic.hpp"

namespace ebus {

constexpr size_t NUM_REQUEST_STATES = 3;

enum class RequestState { firstTry, retrySyn, secondTry };

static const char *getRequestStateText(RequestState state) {
  const char *values[] = {"firstTry", "retrySyn", "secondTry"};
  return values[static_cast<int>(state)];
}

enum class RequestResult {
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
  const char *values[] = {"firstSyn",   "firstWon",   "firstRetry", "firstLost",
                          "firstError", "retrySyn",   "retryError", "secondWon",
                          "secondLost", "secondError"};
  return values[static_cast<int>(result)];
}

#define EBUS_REQUEST_COUNTER_LIST \
  X(requestsTotal)                \
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

  RequestState getState() const;

  void startBit();

  void microsBusIsrDelay(const int64_t &delay);
  void microsBusIsrWindow(const int64_t &window);

  RequestResult run(const uint8_t &address, const uint8_t &byte);

  void resetCounter();
  const Counter &getCounter();

  void resetTiming();
  const Timing &getTiming();

 private:
  std::array<void (Request::*)(const uint8_t &, const uint8_t &),
             NUM_REQUEST_STATES>
      stateRequests;

  RequestState state = RequestState::firstTry;
  RequestResult result = RequestResult::firstSyn;

  // measurement
  Counter counter;
  Timing timing;

  TimingStats busIsrDelay;
  TimingStats busIsrWindow;

  void firstTry(const uint8_t &address, const uint8_t &byte);
  void retrySyn(const uint8_t &address, const uint8_t &byte);
  void secondTry(const uint8_t &address, const uint8_t &byte);

  bool checkPriorityClassSubAddress(const uint8_t &address,
                                    const uint8_t &byte);
};

}  // namespace ebus
