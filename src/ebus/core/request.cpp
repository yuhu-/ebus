/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/request.hpp"

#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"

ebus::Request::Request(BusMonitor* monitor) : monitor_(monitor) {}

void ebus::Request::setMaxLockCounter(uint8_t max_counter) {
  max_lock_counter_ = std::min(max_counter, limits::max_lock_counter);
  if (lock_counter_ > max_lock_counter_) lock_counter_ = max_lock_counter_;
}

uint8_t ebus::Request::getLockCounter() const { return lock_counter_; }

bool ebus::Request::busAvailable() const {
  return result_ == RequestResult::observe_syn && lock_counter_ == 0 &&
         !bus_request_.load(std::memory_order_acquire);
}

bool ebus::Request::requestBus(uint8_t address, bool external) {
  if (busAvailable()) {
    request_address_ = address;
    external_bus_request_ = external;
    // Set flag after data is ready (Release semantics)
    bus_request_.store(true, std::memory_order_release);
  } else if (monitor_) {
    monitor_->updateRequest([](auto& m) { m.bus_request_blocked++; });
  }
  return bus_request_.load(std::memory_order_acquire);
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
  if (monitor_)
    monitor_->updateRequest([](auto& m) { m.lock_counter_reset++; });
  bus_request_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
}

ebus::RequestResult ebus::Request::run(uint8_t byte) {
  size_t idx = static_cast<size_t>(state_);
  if (idx < num_request_states && kStateRequests[idx])
    (this->*kStateRequests[idx])(byte);

  return result_;
}

void ebus::Request::observe(uint8_t byte) {
  if (byte == Protocol::sym_syn) {
    if (lock_counter_ > 0) lock_counter_--;
    result_ = RequestResult::observe_syn;
  } else {
    result_ = RequestResult::observe_data;
  }
}

void ebus::Request::first(uint8_t byte) {
  if (byte == Protocol::sym_syn) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.first_syn++; });
    state_ = RequestState::first;
    result_ = RequestResult::first_syn;
  } else if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.first_won++; });
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
      if (monitor_) monitor_->updateRequest([](auto& m) { m.first_retry++; });
      state_ = RequestState::retry;
      // CRITICAL: We must re-arm the bus request immediately here.
      // If we wait until we see the next SYN in 'retry()', the Bus thread
      // will have already passed the write window for that SYN.
      bus_request_.store(true, std::memory_order_release);
      result_ = RequestResult::first_retry;
    } else {
      if (monitor_) monitor_->updateRequest([](auto& m) { m.first_lost++; });
      state_ = RequestState::observe;
      result_ = RequestResult::first_lost;
    }
  } else {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.first_error++; });
    state_ = RequestState::observe;
    result_ = RequestResult::first_error;
  }
}

void ebus::Request::retry(uint8_t byte) {
  if (byte == Protocol::sym_syn) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.retry_syn++; });
    state_ = RequestState::second;
    result_ = RequestResult::retry_syn;
  } else {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.retry_error++; });
    state_ = RequestState::observe;
    result_ = RequestResult::retry_error;
  }
}

void ebus::Request::second(uint8_t byte) {
  if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.second_won++; });
    lock_counter_ = max_lock_counter_;
    state_ = RequestState::observe;
    result_ = RequestResult::second_won;
  } else if (isMaster(byte)) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.second_lost++; });
    state_ = RequestState::observe;
    result_ = RequestResult::second_lost;
  } else {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.second_error++; });
    state_ = RequestState::observe;
    result_ = RequestResult::second_error;
  }
}
