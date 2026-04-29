/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/request.hpp"

#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"

namespace ebus::detail {

Request::Request(BusMonitor* monitor) : monitor_(monitor) {}

void Request::setLockCounter(uint8_t lock_counter) {
  lock_counter_max_ = std::min(lock_counter, RequestLimits::lock_counter_max);
  if (lock_counter_ > lock_counter_max_) lock_counter_ = lock_counter_max_;
}

uint8_t Request::getLockCounter() const { return lock_counter_; }

bool Request::busAvailable() const {
  return result_ == RequestResult::observe_syn && lock_counter_ == 0 &&
         !bus_request_.load(std::memory_order_acquire);
}

bool Request::requestBus(uint8_t address, bool external) {
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

void Request::setHandlerBusRequestedCallback(BusRequestedCallback callback) {
  handler_bus_requested_callback_ = std::move(callback);
}

void Request::setExternalBusRequestedCallback(BusRequestedCallback callback) {
  external_bus_requested_callback_ = std::move(callback);
}

void Request::busRequestCompleted() {
  bus_request_.store(false, std::memory_order_release);
  if (state_ == RequestState::observe) state_ = RequestState::first;

  if (external_bus_request_) {
    if (external_bus_requested_callback_) external_bus_requested_callback_();
  } else {
    if (handler_bus_requested_callback_) handler_bus_requested_callback_();
  }
}

void Request::startBit() {
  // This is typically called on a bus error, like a framing error, which
  // could be caused by a spurious start bit from an interference impulse.
  // We can no longer trust the bus state, so we abort any pending request
  // and return to the safest state: observing the bus for a clean SYN.
  bus_request_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
  result_ = RequestResult::observe_syn;
}

void Request::setStartBitCallback(StartBitCallback callback) {
  start_bit_callback_ = std::move(callback);
}

ebus::RequestState Request::getState() const { return state_; }

ebus::RequestResult Request::getResult() const { return result_; }

void Request::reset() {
  lock_counter_ = lock_counter_max_;
  if (monitor_)
    monitor_->updateRequest([](auto& m) { m.lock_counter_reset++; });
  bus_request_.store(false, std::memory_order_release);
  state_ = RequestState::observe;
}

ebus::RequestResult Request::run(uint8_t byte) {
  size_t idx = static_cast<size_t>(state_);
  if (idx < FsmLimits::num_request_states && kStateRequests[idx])
    (this->*kStateRequests[idx])(byte);

  return result_;
}

void Request::observe(uint8_t byte) {
  if (byte == Symbols::syn) {
    // Spec 6.4: Decrement unless the SYN follows an arbitration without a clear
    // winner. An arbitration collision results in exactly one byte (the
    // address) between SYNs.
    if (bytes_since_syn_ != 1) {
      if (lock_counter_ > 0) lock_counter_--;
    }
    result_ = RequestResult::observe_syn;
    bytes_since_syn_ = 0;
  } else {
    result_ = RequestResult::observe_data;
    bytes_since_syn_++;
  }
}

void Request::first(uint8_t byte) {
  if (byte == Symbols::syn) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.first_syn++; });
    state_ = RequestState::first;
    result_ = RequestResult::first_syn;
  } else if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.first_won++; });
    lock_counter_ = lock_counter_max_;
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

void Request::retry(uint8_t byte) {
  if (byte == Symbols::syn) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.retry_syn++; });
    state_ = RequestState::second;
    result_ = RequestResult::retry_syn;
  } else {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.retry_error++; });
    state_ = RequestState::observe;
    result_ = RequestResult::retry_error;
  }
}

void Request::second(uint8_t byte) {
  if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.second_won++; });
    lock_counter_ = lock_counter_max_;
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

}  // namespace ebus::detail
