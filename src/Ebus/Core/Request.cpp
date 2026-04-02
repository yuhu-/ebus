/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Core/Request.hpp"

#include "Utils/Common.hpp"

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
         !busRequest_.load(std::memory_order_acquire);
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

ebus::RequestResult ebus::Request::getResult() const {
  if (static_cast<int>(forcedResult_) != -1) return forcedResult_;
  return result_;
}

void ebus::Request::forceResultForTest(RequestResult result) {
  forcedResult_ = result;
}

void ebus::Request::clearForcedResult() {
  forcedResult_ = static_cast<RequestResult>(-1);
}

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

void ebus::Request::resetMetrics() {
#define X(name) counter_.name##_ = 0;
  EBUS_REQUEST_COUNTER_LIST
#undef X
}

std::map<std::string, ebus::MetricValues> ebus::Request::getMetrics() const {
  std::map<std::string, MetricValues> m;
  auto addCounter = [&](const std::string& name, uint32_t val) {
    m["request.counter." + name] = {static_cast<double>(val),  0, 0, 0, 0,
                                    static_cast<uint64_t>(val)};
  };

  // 1. Calculate and map Counters
  Counter c = counter_;

  uint32_t attempts = c.firstWon_ + c.firstLost_ + c.firstRetry_;
  uint32_t collisions = c.firstLost_ + c.firstRetry_;

  addCounter("wonTotal", c.firstWon_ + c.secondWon_);
  addCounter("lostTotal", c.firstLost_ + c.secondLost_);

  // 2. Calculate Contention Rate (%)
  // Contention happens when we lose arbitration (lost or retry) on the first
  // attempt.
  if (attempts > 0) {
    double contentionRate =
        (static_cast<double>(collisions) / attempts) * 100.0;
    m["request.contentionRate"] = {
        contentionRate, contentionRate, contentionRate, contentionRate, 0.0, 1};
  } else {
    m["request.contentionRate"] = {0.0, 0.0, 0.0, 0.0, 0.0, 0};
  }

#define X(name) addCounter(#name, c.name##_);
  EBUS_REQUEST_COUNTER_LIST
#undef X

  return m;
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
    counter_.firstSyn_++;
    state_ = RequestState::first;
    result_ = RequestResult::firstSyn;
  } else if (byte == requestAddress_) {
    counter_.firstWon_++;
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
      counter_.firstRetry_++;
      state_ = RequestState::retry;
      // CRITICAL: We must re-arm the bus request immediately here.
      // If we wait until we see the next SYN in 'retry()', the Bus thread
      // will have already passed the write window for that SYN.
      busRequest_.store(true, std::memory_order_release);
      result_ = RequestResult::firstRetry;
    } else {
      counter_.firstLost_++;
      state_ = RequestState::observe;
      result_ = RequestResult::firstLost;
    }
  } else {
    counter_.firstError_++;
    state_ = RequestState::observe;
    result_ = RequestResult::firstError;
  }
}

void ebus::Request::retry(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter_.retrySyn_++;
    state_ = RequestState::second;
    result_ = RequestResult::retrySyn;
  } else {
    counter_.retryError_++;
    state_ = RequestState::observe;
    result_ = RequestResult::retryError;
  }
}

void ebus::Request::second(const uint8_t& byte) {
  if (byte == requestAddress_) {
    counter_.secondWon_++;
    lockCounter_ = maxLockCounter_;
    state_ = RequestState::observe;
    result_ = RequestResult::secondWon;
  } else if (isMaster(byte)) {
    counter_.secondLost_++;
    state_ = RequestState::observe;
    result_ = RequestResult::secondLost;
  } else {
    counter_.secondError_++;
    state_ = RequestState::observe;
    result_ = RequestResult::secondError;
  }
}
