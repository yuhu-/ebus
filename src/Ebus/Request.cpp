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

#include "Request.hpp"

#include "Common.hpp"

ebus::Request::Request()
    : stateRequests(
          {&Request::firstTry, &Request::retrySyn, &Request::secondTry}) {}

void ebus::Request::setMaxLockCounter(const uint8_t &counter) {
  if (counter > MAX_LOCK_COUNTER)
    maxLockCounter = DEFAULT_LOCK_COUNTER;
  else
    maxLockCounter = counter;
}

void ebus::Request::setLockCounter(const uint8_t &counter) {
  if (counter > maxLockCounter)
    lockCounter = DEFAULT_LOCK_COUNTER;
  else
    lockCounter = counter;
}

const uint8_t ebus::Request::getLockCounter() const { return lockCounter; }

void ebus::Request::resetLockCounter() { lockCounter = maxLockCounter; }

void ebus::Request::handleLockCounter(const uint8_t &byte) {
  if (byte == sym_syn && lockCounter > 0) lockCounter--;
}

ebus::RequestState ebus::Request::getState() const { return state; }

void ebus::Request::countStartBit() {
  counter.requestsStartBit++;
  state = RequestState::firstTry;
  result = RequestResult::firstSyn;
}

void ebus::Request::microsLastDelay(const int64_t &delay) {
  busIsrDelay.add(delay);
}

void ebus::Request::microsLastWindow(const int64_t &window) {
  busIsrWindow.add(window);
}

ebus::RequestResult ebus::Request::run(const uint8_t &address,
                                       const uint8_t &byte) {
  size_t idx = static_cast<size_t>(state);
  if (idx < stateRequests.size() && stateRequests[idx])
    (this->*stateRequests[idx])(address, byte);

  return result;
}

void ebus::Request::resetCounter() {
#define X(name) counter.name = 0;
  EBUS_REQUEST_COUNTER_LIST
#undef X
}

const ebus::Request::Counter &ebus::Request::getCounter() {
  counter.requestsTotal =
      counter.requestsStartBit + counter.requestsFirstSyn +
      counter.requestsFirstWon + counter.requestsFirstRetry +
      counter.requestsFirstLost + counter.requestsFirstError +
      counter.requestsRetrySyn + +counter.requestsRetryError +
      counter.requestsSecondWon + counter.requestsSecondLost +
      counter.requestsSecondError;

  return counter;
}

void ebus::Request::resetTiming() {
  busIsrDelay.clear();
  busIsrWindow.clear();
}

const ebus::Request::Timing &ebus::Request::getTiming() {
#define X(name)                    \
  timing.name##Last = name.last;   \
  timing.name##Count = name.count; \
  timing.name##Mean = name.mean;   \
  timing.name##StdDev = name.stddev();
  EBUS_REQUEST_TIMING_LIST
#undef X
  return timing;
}

void ebus::Request::firstTry(const uint8_t &address, const uint8_t &byte) {
  if (byte == sym_syn) {
    counter.requestsFirstSyn++;
    result = RequestResult::firstSyn;
  } else if (byte == address) {
    counter.requestsFirstWon++;
    lockCounter = maxLockCounter;
    result = RequestResult::firstWon;
  } else if (isMaster(byte)) {
    if (checkPriorityClassSubAddress(address, byte)) {
      counter.requestsFirstRetry++;
      state = RequestState::retrySyn;  // switch to retry state
      result = RequestResult::firstRetry;
    } else {
      counter.requestsFirstLost++;
      result = RequestResult::firstLost;
    }
  } else {
    counter.requestsFirstError++;
    result = RequestResult::firstError;
  }
}

void ebus::Request::retrySyn(const uint8_t &address, const uint8_t &byte) {
  if (byte == sym_syn) {
    counter.requestsRetrySyn++;
    state = RequestState::secondTry;  // switch to second try state
    result = RequestResult::retrySyn;
  } else {
    counter.requestsRetryError++;
    state = RequestState::firstTry;  // reset to first try state
    result = RequestResult::retryError;
  }
}

void ebus::Request::secondTry(const uint8_t &address, const uint8_t &byte) {
  if (byte == address) {
    counter.requestsSecondWon++;
    lockCounter = maxLockCounter;
    result = RequestResult::secondWon;
  } else if (isMaster(byte)) {
    counter.requestsSecondLost++;
    result = RequestResult::secondLost;
  } else {
    counter.requestsSecondError++;
    result = RequestResult::secondError;
  }
  state = RequestState::firstTry;  // reset to first try state
}

// check priority class (lower nibble) and sub address (higher nibble)
bool ebus::Request::checkPriorityClassSubAddress(const uint8_t &address,
                                                 const uint8_t &byte) {
  return (byte & 0x0f) == (address & 0x0f) &&  // priority class
         (byte & 0xf0) > (address & 0xf0);     // sub address
}
