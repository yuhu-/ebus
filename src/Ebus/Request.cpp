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

#include "Request.hpp"

#include "Common.hpp"

ebus::Request::Request()
    : stateRequests({&Request::observe, &Request::first, &Request::retry,
                     &Request::second}) {}

void ebus::Request::setMaxLockCounter(const uint8_t& maxCounter) {
  if (maxCounter > MAX_LOCK_COUNTER)
    maxLockCounter = MAX_LOCK_COUNTER;
  else
    maxLockCounter = maxCounter;

  if (lockCounter > maxLockCounter) lockCounter = maxLockCounter;
}

const uint8_t ebus::Request::getLockCounter() const { return lockCounter; }

uint8_t ebus::Request::getAddress() const { return sourceAddress; }

bool ebus::Request::busAvailable() const {
  return result == RequestResult::observeSyn && lockCounter == 0 && !busRequest;
}

bool ebus::Request::requestBus(const uint8_t& address, const bool& external) {
  if (busAvailable()) {
    busRequest = true;
    sourceAddress = address;
    externalBusRequest = external;
  }
  return busRequest;
}

void ebus::Request::setHandlerBusRequestedCallback(
    BusRequestedCallback callback) {
  handlerBusRequestedCallback = std::move(callback);
}

void ebus::Request::setExternalBusRequestedCallback(
    BusRequestedCallback callback) {
  externalBusRequestedCallback = std::move(callback);
}

bool ebus::Request::busRequestPending() const { return busRequest; }

void ebus::Request::busRequestCompleted() {
  busRequest = false;
  if (state == RequestState::observe) state = RequestState::first;

  if (externalBusRequest) {
    if (externalBusRequestedCallback) externalBusRequestedCallback();
  } else {
    if (handlerBusRequestedCallback) handlerBusRequestedCallback();
  }
}

void ebus::Request::startBit() {
  counter.requestsStartBit++;
  state = RequestState::observe;
  result = RequestResult::observeSyn;
  if (startBitCallback) startBitCallback();
}

void ebus::Request::setStartBitCallback(StartBitCallback callback) {
  startBitCallback = std::move(callback);
}

ebus::RequestState ebus::Request::getState() const { return state; }

ebus::RequestResult ebus::Request::getResult() const { return result; }

void ebus::Request::reset() {
  lockCounter = maxLockCounter;
  busRequest = false;
  state = RequestState::observe;
}

ebus::RequestResult ebus::Request::run(const uint8_t& byte) {
  size_t idx = static_cast<size_t>(state);
  if (idx < stateRequests.size() && stateRequests[idx])
    (this->*stateRequests[idx])(byte);

  return result;
}

void ebus::Request::microsLastDelay(const int64_t& delay) {
  busIsrDelay.addDuration(delay);
}

void ebus::Request::microsLastWindow(const int64_t& window) {
  busIsrWindow.addDuration(window);
}

void ebus::Request::resetCounter() {
#define X(name) counter.name = 0;
  EBUS_REQUEST_COUNTER_LIST
#undef X
}

const ebus::Request::Counter& ebus::Request::getCounter() const {
  return counter;
}

void ebus::Request::resetTiming() {
  busIsrDelay.clear();
  busIsrWindow.clear();
}

const ebus::Request::Timing& ebus::Request::getTiming() {
#define X(name)                          \
  {                                      \
    auto values = name.getValues();            \
    timing.name##Last = values.last;     \
    timing.name##Count = values.count;   \
    timing.name##Mean = values.mean;     \
    timing.name##StdDev = values.stddev; \
  }
  EBUS_REQUEST_TIMING_LIST
#undef X
  return timing;
}

void ebus::Request::observe(const uint8_t& byte) {
  if (byte == sym_syn) {
    if (lockCounter > 0) lockCounter--;
    result = RequestResult::observeSyn;
  } else {
    result = RequestResult::observeData;
  }
}

void ebus::Request::first(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter.requestsFirstSyn++;
    result = RequestResult::firstSyn;
  } else if (byte == sourceAddress) {
    counter.requestsFirstWon++;
    lockCounter = maxLockCounter;
    state = RequestState::observe;
    result = RequestResult::firstWon;
  } else if (isMaster(byte)) {
    if (checkPriorityClassSubAddress(byte)) {
      counter.requestsFirstRetry++;
      state = RequestState::retry;
      result = RequestResult::firstRetry;
    } else {
      counter.requestsFirstLost++;
      state = RequestState::observe;
      result = RequestResult::firstLost;
    }
  } else {
    counter.requestsFirstError++;
    state = RequestState::observe;
    result = RequestResult::firstError;
  }
}

void ebus::Request::retry(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter.requestsRetrySyn++;
    busRequest = true;
    state = RequestState::second;
    result = RequestResult::retrySyn;
  } else {
    counter.requestsRetryError++;
    state = RequestState::observe;
    result = RequestResult::retryError;
  }
}

void ebus::Request::second(const uint8_t& byte) {
  if (byte == sourceAddress) {
    counter.requestsSecondWon++;
    lockCounter = maxLockCounter;
    state = RequestState::observe;
    result = RequestResult::secondWon;
  } else if (isMaster(byte)) {
    counter.requestsSecondLost++;
    state = RequestState::observe;
    result = RequestResult::secondLost;
  } else {
    counter.requestsSecondError++;
    state = RequestState::observe;
    result = RequestResult::secondError;
  }
}

// check priority class (lower nibble) and sub address (higher nibble)
bool ebus::Request::checkPriorityClassSubAddress(const uint8_t& byte) {
  return (byte & 0x0f) == (sourceAddress & 0x0f) &&  // priority class
         (byte & 0xf0) > (sourceAddress & 0xf0);     // sub address
}
