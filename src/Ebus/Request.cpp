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
    : stateRequests_({&Request::observe, &Request::first, &Request::retry,
                      &Request::second}) {}

void ebus::Request::setMaxLockCounter(const uint8_t& maxCounter) {
  if (maxCounter > MAX_LOCK_COUNTER)
    maxLockCounter_ = MAX_LOCK_COUNTER;
  else
    maxLockCounter_ = maxCounter;

  if (lockCounter_ > maxLockCounter_) lockCounter_ = maxLockCounter_;
}

uint8_t ebus::Request::getLockCounter() const { return lockCounter_; }

bool ebus::Request::busAvailable() const {
  return result_ == RequestResult::observeSyn && lockCounter_ == 0 &&
         !busRequest_;
}

bool ebus::Request::requestBus(const uint8_t& address, const bool& external) {
  if (busAvailable()) {
    requestAddress_ = address;
    externalBusRequest_ = external;
    // Set flag after data is ready (Release semantics)
    busRequest_.store(true, std::memory_order_release);
  }
  return busRequest_;
}

void ebus::Request::setHandlerBusRequestedCallback(
    BusRequestedCallback callback) {
  handlerBusRequestedCallback_ = std::move(callback);
}

void ebus::Request::setExternalBusRequestedCallback(
    BusRequestedCallback callback) {
  externalBusRequestedCallback_ = std::move(callback);
}

void ebus::Request::busRequestCompleted() {
  busRequest_.store(false, std::memory_order_release);
  if (state_ == RequestState::observe) state_ = RequestState::first;

  if (externalBusRequest_) {
    if (externalBusRequestedCallback_) externalBusRequestedCallback_();
  } else {
    if (handlerBusRequestedCallback_) handlerBusRequestedCallback_();
  }
}

void ebus::Request::startBit() {
  // This is typically called on a bus error, like a framing error, which could
  // be caused by a spurious start bit from an interference impulse. We can no
  // longer trust the bus state, so we abort any pending request and return to
  // the safest state: observing the bus for a clean SYN.
  busRequest_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
  result_ = RequestResult::observeSyn;
}

void ebus::Request::setStartBitCallback(StartBitCallback callback) {
  startBitCallback_ = std::move(callback);
}

ebus::RequestState ebus::Request::getState() const { return state_; }

ebus::RequestResult ebus::Request::getResult() const { return result_; }

void ebus::Request::reset() {
  lockCounter_ = maxLockCounter_;
  busRequest_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
}

ebus::RequestResult ebus::Request::run(const uint8_t& byte) {
  size_t idx = static_cast<size_t>(state_);
  if (idx < stateRequests_.size() && stateRequests_[idx])
    (this->*stateRequests_[idx])(byte);

  return result_;
}

void ebus::Request::resetCounter() {
#define X(name) counter_.name = 0;
  EBUS_REQUEST_COUNTER_LIST
#undef X
}

const ebus::Request::Counter& ebus::Request::getCounter() const {
  return counter_;
}

void ebus::Request::observe(const uint8_t& byte) {
  if (byte == sym_syn) {
    if (lockCounter_ > 0) lockCounter_--;
    result_ = RequestResult::observeSyn;
  } else {
    result_ = RequestResult::observeData;
  }
}

void ebus::Request::first(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter_.requestsFirstSyn++;
    state_ = RequestState::first;
    result_ = RequestResult::firstSyn;
  } else if (byte == requestAddress_) {
    counter_.requestsFirstWon++;
    lockCounter_ = maxLockCounter_;
    state_ = RequestState::observe;
    result_ = RequestResult::firstWon;
  } else if (isMaster(byte)) {
    // ARBITRATION LOSS (Wire-AND Logic):
    // We sent our address, but read back a different master address.
    // Since '0' dominates '1', we know we lost to a higher priority (lower
    // value) address or a peer with the same bits set to '0'.
    //
    // SPECIAL RETRY CASE (Spec 6.2.2.2):
    // If the Priority Class (Bits 0-3) matches our own, we are allowed
    // to retry immediately at the next SYN (Auto-SYN).
    if ((byte & 0x0f) == (requestAddress_ & 0x0f)) {
      counter_.requestsFirstRetry++;
      state_ = RequestState::retry;
      // CRITICAL: We must re-arm the bus request immediately here.
      // If we wait until we see the next SYN in 'retry()', the Bus thread
      // will have already passed the write window for that SYN.
      busRequest_.store(true, std::memory_order_release);
      result_ = RequestResult::firstRetry;
    } else {
      counter_.requestsFirstLost++;
      state_ = RequestState::observe;
      result_ = RequestResult::firstLost;
    }
  } else {
    counter_.requestsFirstError++;
    state_ = RequestState::observe;
    result_ = RequestResult::firstError;
  }
}

void ebus::Request::retry(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter_.requestsRetrySyn++;
    state_ = RequestState::second;
    result_ = RequestResult::retrySyn;
  } else {
    counter_.requestsRetryError++;
    state_ = RequestState::observe;
    result_ = RequestResult::retryError;
  }
}

void ebus::Request::second(const uint8_t& byte) {
  if (byte == requestAddress_) {
    counter_.requestsSecondWon++;
    lockCounter_ = maxLockCounter_;
    state_ = RequestState::observe;
    result_ = RequestResult::secondWon;
  } else if (isMaster(byte)) {
    counter_.requestsSecondLost++;
    state_ = RequestState::observe;
    result_ = RequestResult::secondLost;
  } else {
    counter_.requestsSecondError++;
    state_ = RequestState::observe;
    result_ = RequestResult::secondError;
  }
}
