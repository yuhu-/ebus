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
  firstLost,
  firstRetry,
  firstError,
  retrySyn,
  retryError,
  secondWon,
  secondLost,
  secondError
};

static const char *getRequestResultText(RequestResult result) {
  const char *values[] = {
      "firstSyn", "firstWon",   "firstLost", "firstRetry", "firstError",
      "retrySyn", "retryError", "secondWon", "secondLost", "secondError"};
  return values[static_cast<int>(result)];
}

class Request {
 public:
  explicit Request();

  RequestState getState() const;

  RequestResult run(const uint8_t &address, const uint8_t &byte);

 private:
  std::array<void (Request::*)(const uint8_t &, const uint8_t &),
             NUM_REQUEST_STATES>
      stateRequests;

  RequestState state = RequestState::firstTry;
  RequestResult result = RequestResult::firstSyn;

  void firstTry(const uint8_t &address, const uint8_t &byte);
  void retrySyn(const uint8_t &address, const uint8_t &byte);
  void secondTry(const uint8_t &address, const uint8_t &byte);

  bool checkPriorityClassSubAddress(const uint8_t &address,
                                    const uint8_t &byte);
};

}  // namespace ebus
