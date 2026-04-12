/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/request.hpp"

#include "utils/common.hpp"

ebus::Request::Request()
    : state_requests_({&Request::observe, &Request::first, &Request::retry,
                       &Request::second}) {}

void ebus::Request::setMaxLockCounter(const uint8_t& max_counter) {
  if (max_counter > MAX_LOCK_COUNTER)
    max_lock_counter_ = MAX_LOCK_COUNTER;
  else
    max_lock_counter_ = max_counter;

  if (lock_counter_ > max_lock_counter_) lock_counter_ = max_lock_counter_;
}

uint8_t ebus::Request::getLockCounter() const { return lock_counter_; }

bool ebus::Request::busAvailable() const {
  return result_ == RequestResult::observe_syn && lock_counter_ == 0 &&
         !bus_request_.load(std::memory_order_acquire);
}

bool ebus::Request::requestBus(const uint8_t& address, const bool& external) {
  if (busAvailable()) {
    request_address_ = address;
    external_bus_request_ = external;
    // Set flag after data is ready (Release semantics)
    bus_request_.store(true, std::memory_order_release);
  }
  return bus_request_;
}

void ebus::Request::setHandlerBusRequestedCallback(
    BusRequestedCallback callback) {
  handler_bus_requested_callback_ = std::move(callback);
}

void ebus::Request::setExternalBusRequestedCallback(
    BusRequestedCallback callback) {
  external_bus_requested_callback_ = std::move(callback);
}

void ebus::Request::busRequestCompleted() {
  bus_request_.store(false, std::memory_order_release);
  if (state_ == RequestState::observe) state_ = RequestState::first;

  if (external_bus_request_) {
    if (external_bus_requested_callback_) external_bus_requested_callback_();
  } else {
    if (handler_bus_requested_callback_) handler_bus_requested_callback_();
  }
}

void ebus::Request::startBit() {
  // This is typically called on a bus error, like a framing error, which
  // could be caused by a spurious start bit from an interference impulse.
  // We can no longer trust the bus state, so we abort any pending request
  // and return to the safest state: observing the bus for a clean SYN.
  bus_request_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
  result_ = RequestResult::observe_syn;
}

void ebus::Request::setStartBitCallback(StartBitCallback callback) {
  start_bit_callback_ = std::move(callback);
}

ebus::RequestState ebus::Request::getState() const { return state_; }

ebus::RequestResult ebus::Request::getResult() const { return result_; }

void ebus::Request::reset() {
  lock_counter_ = max_lock_counter_;
  bus_request_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
}

ebus::RequestResult ebus::Request::run(const uint8_t& byte) {
  size_t idx = static_cast<size_t>(state_);
  if (idx < state_requests_.size() && state_requests_[idx])
    (this->*state_requests_[idx])(byte);

  return result_;
}

void ebus::Request::resetMetrics() {
#define X(name) counter_.name##_ = 0;
  EBUS_REQUEST_COUNTER_LIST
#undef X
}

std::map<std::string, ebus::MetricValues> ebus::Request::getMetrics() const {
  std::map<std::string, MetricValues> m;
  auto add_counter = [&](const std::string& name, uint32_t val) {
    m["request.counter." + name] = {static_cast<double>(val),  0, 0, 0, 0,
                                    static_cast<uint64_t>(val)};
  };

  // 1. Calculate and map Counters
  Counter c = counter_;

  uint32_t attempts = c.first_won_ + c.first_lost_ + c.first_retry_;
  uint32_t collisions = c.first_lost_ + c.first_retry_;

  add_counter("won_total", c.first_won_ + c.second_won_);
  add_counter("lost_total", c.first_lost_ + c.second_lost_);

  // 2. Calculate Contention Rate (%)
  // Contention happens when we lose arbitration (lost or retry) on the
  // first attempt.
  if (attempts > 0) {
    double contention_rate =
        (static_cast<double>(collisions) / attempts) * 100.0;
    m["request.contention_rate"] = {contention_rate,
                                    contention_rate,
                                    contention_rate,
                                    contention_rate,
                                    0.0,
                                    1};
  } else {
    m["request.contentionRate"] = {0.0, 0.0, 0.0, 0.0, 0.0, 0};
  }

#define X(name) add_counter(#name, c.name##_);
  EBUS_REQUEST_COUNTER_LIST
#undef X

  return m;
}

void ebus::Request::observe(const uint8_t& byte) {
  if (byte == sym_syn) {
    if (lock_counter_ > 0) lock_counter_--;
    result_ = RequestResult::observe_syn;
  } else {
    result_ = RequestResult::observe_data;
  }
}

void ebus::Request::first(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter_.first_syn_++;
    state_ = RequestState::first;
    result_ = RequestResult::first_syn;
  } else if (byte == request_address_) {
    counter_.first_won_++;
    lock_counter_ = max_lock_counter_;
    state_ = RequestState::observe;
    result_ = RequestResult::first_won;
  } else if (isMaster(byte)) {
    // ARBITRATION LOSS (Wire-AND Logic):
    // We sent our address, but read back a different master address.
    // Since '0' dominates '1', we know we lost to a higher priority (lower
    // value) address or a peer with the same bits set to '0'.
    //
    // SPECIAL RETRY CASE (Spec 6.2.2.2):
    // If the Priority Class (Bits 0-3) matches our own, we are allowed
    // to retry immediately at the next SYN (Auto-SYN).
    if ((byte & 0x0f) == (request_address_ & 0x0f)) {
      counter_.first_retry_++;
      state_ = RequestState::retry;
      // CRITICAL: We must re-arm the bus request immediately here.
      // If we wait until we see the next SYN in 'retry()', the Bus thread
      // will have already passed the write window for that SYN.
      bus_request_.store(true, std::memory_order_release);
      result_ = RequestResult::first_retry;
    } else {
      counter_.first_lost_++;
      state_ = RequestState::observe;
      result_ = RequestResult::first_lost;
    }
  } else {
    counter_.first_error_++;
    state_ = RequestState::observe;
    result_ = RequestResult::first_error;
  }
}

void ebus::Request::retry(const uint8_t& byte) {
  if (byte == sym_syn) {
    counter_.retry_syn_++;
    state_ = RequestState::second;
    result_ = RequestResult::retry_syn;
  } else {
    counter_.retry_error_++;
    state_ = RequestState::observe;
    result_ = RequestResult::retry_error;
  }
}

void ebus::Request::second(const uint8_t& byte) {
  if (byte == request_address_) {
    counter_.second_won_++;
    lock_counter_ = max_lock_counter_;
    state_ = RequestState::observe;
    result_ = RequestResult::second_won;
  } else if (isMaster(byte)) {
    counter_.second_lost_++;
    state_ = RequestState::observe;
    result_ = RequestResult::second_lost;
  } else {
    counter_.second_error_++;
    state_ = RequestState::observe;
    result_ = RequestResult::second_error;
  }
}
