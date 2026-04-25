/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/defaults.hpp>
#include <ebus/metrics.hpp>
#include <ebus/types.hpp>
#include <functional>

namespace ebus {

class BusMonitor;

using BusRequestedCallback = std::function<void()>;
using StartBitCallback = std::function<void()>;

/**
 * Implementation of the eBUS arbitration state machine.
 * Handles "Wire-AND" collision detection, Priority Class checks, and the
 * automatic retry mechanism (Auto-SYN) as defined in Spec 6.2.2.
 */
class Request {
 public:
  explicit Request(BusMonitor* monitor = nullptr);

  void setMaxLockCounter(uint8_t max_counter);
  uint8_t getLockCounter() const;

  bool busAvailable() const;

  // Request the bus from handler or external
  bool requestBus(uint8_t address, bool external = false);

  void setHandlerBusRequestedCallback(BusRequestedCallback callback);
  void setExternalBusRequestedCallback(BusRequestedCallback callback);

  // Inline and non-virtual for ESP32 ISR safety (IRAM) and performance
  inline uint8_t busRequestAddress() const { return request_address_; }

  inline bool busRequestPending() const {
    return bus_request_.load(std::memory_order_acquire);
  }

  void busRequestCompleted();

  void startBit();
  void setStartBitCallback(StartBitCallback callback);

  RequestState getState() const;
  RequestResult getResult() const;

  void reset();

  RequestResult run(uint8_t byte);

 private:
  BusMonitor* monitor_ = nullptr;

  uint8_t max_lock_counter_ = defaults::Arbitration::lock_counter;
  uint8_t lock_counter_ = defaults::Arbitration::lock_counter;

  uint8_t request_address_ = 0;

  // Indicates whether a bus request is present
  std::atomic<bool> bus_request_ = {false};

  // Indicates whether the bus request is internal or external
  bool external_bus_request_ = false;

  BusRequestedCallback handler_bus_requested_callback_ = nullptr;
  BusRequestedCallback external_bus_requested_callback_ = nullptr;

  // TODO fix this - this will reset the handler - do we need this?
  StartBitCallback start_bit_callback_ = nullptr;

  void observe(uint8_t byte);
  void first(uint8_t byte);
  void retry(uint8_t byte);
  void second(uint8_t byte);

  using StateHandler = void (Request::*)(uint8_t);
  static inline constexpr StateHandler kStateRequests[] = {
      &Request::observe, &Request::first, &Request::retry, &Request::second};

  static_assert(sizeof(kStateRequests) / sizeof(kStateRequests[0]) ==
                    ebus::FSM::num_request_states,
                "kStateRequests table size does not match NUM_REQUEST_STATES");

  RequestState state_ = RequestState::observe;
  RequestResult result_ = RequestResult::observe_syn;
};

}  // namespace ebus
