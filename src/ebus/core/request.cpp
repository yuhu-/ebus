/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/request.hpp"

#include <algorithm>
#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"

namespace ebus::detail {

namespace {
template <typename... Args>
constexpr uint8_t mask(Args... states) {
  if constexpr (sizeof...(Args) == 0) {
    return 0;
  } else {
    return (... | (1 << static_cast<int>(states)));
  }
}

// FSM Transition Matrix (Arbitration)
static constexpr uint8_t transition_masks[] = {
    // 0: observe
    mask(RequestState::observe, RequestState::first),
    // 1: first
    mask(RequestState::observe, RequestState::first, RequestState::retry),
    // 2: retry
    mask(RequestState::observe, RequestState::second),
    // 3: second
    mask(RequestState::observe)};
}  // namespace

Request::Request(BusMonitor* monitor) : monitor_(monitor) {}

void Request::reset() {
  lock_counter_ = lock_counter_max_;
  bytes_since_syn_ = 0;
  if (monitor_)
    monitor_->updateRequest([](auto& m) { m.lock_counter_reset++; });
  bus_request_.store(false, std::memory_order_release);
  transitionTo(RequestState::observe);
}

void Request::setLockCounter(uint8_t lock_counter) {
  lock_counter_max_ = std::min(lock_counter, RequestLimits::lock_counter_max);
  if (lock_counter_ > lock_counter_max_) lock_counter_ = lock_counter_max_;
}

uint8_t Request::getLockCounter() const { return lock_counter_; }

void Request::setHandlerBusRequestedCallback(BusRequestedCallback callback) {
  handler_bus_requested_callback_ = std::move(callback);
}

void Request::setExternalBusRequestedCallback(BusRequestedCallback callback) {
  external_bus_requested_callback_ = std::move(callback);
}

void Request::setStartBitCallback(StartBitCallback callback) {
  start_bit_callback_ = std::move(callback);
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

void Request::busRequestCompleted() {
  bus_request_.store(false, std::memory_order_release);
  if (state_ == RequestState::observe) transitionTo(RequestState::first);

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
  transitionTo(RequestState::observe);
  result_ = RequestResult::observe_syn;
  if (start_bit_callback_) start_bit_callback_();
}

ebus::RequestResult Request::run(uint8_t byte) {
  size_t idx = static_cast<size_t>(state_);
  if (idx < FsmLimits::num_request_states && state_requests[idx])
    (this->*state_requests[idx])(byte);

  return result_;
}

bool Request::busAvailable() const {
  return result_ == RequestResult::observe_syn && lock_counter_ == 0 &&
         !bus_request_.load(std::memory_order_acquire);
}

ebus::RequestState Request::getState() const { return state_; }

ebus::RequestResult Request::getResult() const { return result_; }

void Request::observe(uint8_t byte) {
  if (byte == Symbols::syn) {
    // Spec 6.4: Decrement unless the SYN follows an arbitration without a clear
    // winner. An arbitration collision results in exactly one byte (the
    // address) between SYNs.
    if (bytes_since_syn_ != RequestLimits::collision_byte_count) {
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
    transitionTo(RequestState::first);
    result_ = RequestResult::first_syn;
    bytes_since_syn_ = 0;
  } else if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.won_total++; });
    lock_counter_ = lock_counter_max_;
    transitionTo(RequestState::observe);
    result_ = RequestResult::first_won;
    bytes_since_syn_ = 0;
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
      if (monitor_) monitor_->updateRequest([](auto& m) { m.collisions++; });
      transitionTo(RequestState::retry);
      // CRITICAL: We must re-arm the bus request immediately here.
      // If we wait until we see the next SYN in 'retry()', the Bus thread
      // will have already passed the write window for that SYN.
      bus_request_.store(true, std::memory_order_release);
      result_ = RequestResult::first_retry;
      bytes_since_syn_ = RequestLimits::collision_byte_count;
    } else {
      if (monitor_) monitor_->updateRequest([](auto& m) { m.lost_total++; });
      transitionTo(RequestState::observe);
      result_ = RequestResult::first_lost;
      bytes_since_syn_ = RequestLimits::collision_byte_count;
    }
  } else {
    if (monitor_)
      monitor_->updateRequest([](auto& m) { m.arbitration_errors++; });
    transitionTo(RequestState::observe);
    result_ = RequestResult::first_error;
    bytes_since_syn_ = RequestLimits::collision_byte_count;
  }
}

void Request::retry(uint8_t byte) {
  if (byte == Symbols::syn) {
    transitionTo(RequestState::second);
    result_ = RequestResult::retry_syn;
    bytes_since_syn_ = 0;
  } else {
    if (monitor_)
      monitor_->updateRequest([](auto& m) { m.arbitration_errors++; });
    transitionTo(RequestState::observe);
    result_ = RequestResult::retry_error;
    bytes_since_syn_ = RequestLimits::collision_byte_count;
  }
}

void Request::second(uint8_t byte) {
  if (byte == request_address_) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.won_total++; });
    lock_counter_ = lock_counter_max_;
    transitionTo(RequestState::observe);
    result_ = RequestResult::second_won;
    bytes_since_syn_ = 0;
  } else if (isMaster(byte)) {
    if (monitor_) monitor_->updateRequest([](auto& m) { m.lost_total++; });
    transitionTo(RequestState::observe);
    result_ = RequestResult::second_lost;
    bytes_since_syn_ = RequestLimits::collision_byte_count;
  } else {
    if (monitor_)
      monitor_->updateRequest([](auto& m) { m.arbitration_errors++; });
    transitionTo(RequestState::observe);
    result_ = RequestResult::second_error;
    bytes_since_syn_ = RequestLimits::collision_byte_count;
  }
}

void Request::transitionTo(RequestState next) {
  if (next == state_) return;

  const RequestState old_state = state_;
  const uint8_t next_bit = 1 << static_cast<int>(next);
  const uint8_t valid_mask = transition_masks[static_cast<size_t>(state_)];

  if (!(next_bit & valid_mask)) {
    // Safe recovery: return to observe state.
    next = RequestState::observe;
  }

  state_ = next;

  if (monitor_) {
    monitor_->logRequestTransition(old_state, next);
  }
}

}  // namespace ebus::detail
