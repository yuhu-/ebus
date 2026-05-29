/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <functional>
#include <mutex>

#include "utils/circular_buffer.hpp"
#include "utils/timing_stats.hpp"

namespace ebus::detail {

/**
 * BusMonitor centralizes timing and performance statistics for the bus.
 * By separating telemetry from protocol logic, we keep the core FSMs clean.
 */
class BusMonitor {
 public:
  BusMonitor();
  void resetMetrics();
  void fetchMetrics(const std::function<void(const Metrics&)>& callback) const;

  using HandlerHistory =
      CircularBuffer<HandlerTransition, FsmLimits::transition_history_size>;
  using RequestHistory =
      CircularBuffer<RequestTransition, FsmLimits::transition_history_size>;
  using UtilizationHistory =
      CircularBuffer<float, DiagnosticsLimits::log_history_size>;

  void fetchHistory(
      const std::function<void(const HandlerHistory&, const RequestHistory&,
                               const UtilizationHistory&)>& callback) const;

  float getBusUtilization() const;
  void updateUtilizationHistory();
  void fetchUtilizationHistory(
      const std::function<void(float)>& callback) const;

  // Thread-safe update helpers
  template <typename F>
  void updateHandler(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    updater(handler_acc_);
  }

  template <typename F>
  void updateRequest(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    updater(request_acc_);
  }

  template <typename F>
  void updateBus(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    updater(bus_acc_);
  }

  template <typename F>
  void updateDevice(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    updater(device_acc_);
  }

  template <typename F>
  void updateController(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    updater(controller_acc_);
  }
  void recordBusError();
  void recordLowBits(uint32_t bits);
  void clearHistory();

  void logHandlerTransition(HandlerState from, HandlerState to);
  void logRequestTransition(RequestState from, RequestState to);

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
  mutable std::mutex metrics_mutex_;

  mutable Clock::time_point congestion_start_point_{};
  mutable bool congestion_active_ = false;
  Clock::time_point uptime_start_{Clock::now()};
  uint64_t total_low_bits_ = 0;
  uint64_t last_history_low_bits_ = 0;
  uint64_t last_history_uptime_us_ = 0;

  metrics::HandlerMetrics handler_acc_;
  metrics::RequestMetrics request_acc_;
  metrics::BusMetrics bus_acc_;
  metrics::DeviceMetrics device_acc_;
  metrics::ControllerMetrics controller_acc_;

  CircularBuffer<HandlerTransition, FsmLimits::transition_history_size>
      handler_history_;
  CircularBuffer<RequestTransition, FsmLimits::transition_history_size>
      request_history_;
  CircularBuffer<float, DiagnosticsLimits::log_history_size>
      utilization_history_;
};

}  // namespace ebus::detail
