/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <ebus/detail/circular_buffer.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <functional>

#include "platform/mutex.hpp"
#include "utils/timing_stats.hpp"

namespace ebus::detail {

/**
 * BusMonitor centralizes timing and performance statistics for the bus.
 * By separating telemetry from protocol logic, we keep the core FSMs clean.
 */
class BusMonitor {
 public:
  // --- Public Types & Constants ---
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  using HandlerHistory =
      CircularBuffer<HandlerTransition, FsmLimits::transition_history_size>;
  using RequestHistory =
      CircularBuffer<RequestTransition, FsmLimits::transition_history_size>;
  using UtilizationHistory =
      CircularBuffer<float, DiagnosticsLimits::log_history_size>;
#endif

  // Lifecycle
  BusMonitor();
  BusMonitor(const BusMonitor&) = delete;
  BusMonitor& operator=(const BusMonitor&) = delete;

  // Working Methods
  void resetMetrics();

  // Thread-safe update helpers
  template <typename F>
  void updateHandler(F&& updater) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    updater(handler_acc_);
  }

  template <typename F>
  void updateRequest(F&& updater) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    updater(request_acc_);
  }

  template <typename F>
  void updateBus(F&& updater) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    updater(bus_acc_);
  }

  template <typename F>
  void updateDevice(F&& updater) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    updater(device_acc_);
  }

  template <typename F>
  void updateController(F&& updater) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    updater(controller_acc_);
  }

  /**
   * @brief Resets the interval-based loop timing peak.
   */
  void resetLoopCycle() {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    controller_acc_.max_loop_cycle_us = 0;
  }

  /**
   * @brief Resets the interval-based max reactor queue size.
   */
  void resetMaxReactorQueueSize(size_t current) {
    platform::LockGuard<platform::Mutex> lock(metrics_mutex_);
    controller_acc_.max_reactor_queue_size = static_cast<uint32_t>(current);
  }

  void recordBusError();
  void recordLowBits(uint32_t bits);
  void recordHandlerError(uint8_t address);
  void recordHandlerSuccess(uint8_t address);
  void recordIsrStartBitError();
  void recordIsrSynPostponed(uint32_t count);

  void updateUtilizationHistory();

  void logPassiveReset();
  void logActiveReset();
  void clearHistory();

  void logHandlerTransition(HandlerState from, HandlerState to);
  void logRequestTransition(RequestState from, RequestState to);

  // Status/Telemetry
  float getBusUtilization() const;
  void fetchMetrics(const std::function<void(const Metrics&)>& callback) const;
  void fetchUtilizationHistory(
      const std::function<void(float)>& callback) const;
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  void fetchHistory(
      const std::function<void(const HandlerHistory&, const RequestHistory&,
                               const UtilizationHistory&)>& callback) const;
#endif

  // Performance Timing Stats (Public for Hot Path optimization)
  TimingStats sync;
  TimingStats write;
  TimingStats passive_first;
  TimingStats passive_data;
  TimingStats active_first;
  TimingStats active_data;

  // Physical Layer stats (moved from BusPosix/BusFreeRtos)
  TimingStats delay;
  TimingStats window;
  TimingStats transmit;
  TimingStats syn_postpone;

 private:
  mutable platform::Mutex metrics_mutex_;

  mutable Clock::time_point congestion_start_point_{};
  mutable bool congestion_active_ = false;
  Clock::time_point uptime_start_{Clock::now()};
  uint64_t total_low_bits_ = 0;

  metrics::HandlerMetrics handler_acc_;
  metrics::RequestMetrics request_acc_;
  metrics::BusMetrics bus_acc_;
  metrics::DeviceMetrics device_acc_;
  metrics::ControllerMetrics controller_acc_;

#ifndef EBUS_MINIMAL_DIAGNOSTICS
  uint64_t last_history_low_bits_ = 0;
  uint64_t last_history_uptime_us_ = 0;

  CircularBuffer<HandlerTransition, FsmLimits::transition_history_size>
      handler_history_;
  CircularBuffer<RequestTransition, FsmLimits::transition_history_size>
      request_history_;
  CircularBuffer<float, DiagnosticsLimits::log_history_size>
      utilization_history_;
#endif
};

}  // namespace ebus::detail
