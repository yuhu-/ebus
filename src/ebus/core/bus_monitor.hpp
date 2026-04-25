/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <functional>
#include <mutex>

#include "core/constants.hpp"
#include "ebus/metrics.hpp"
#include "utils/circular_buffer.hpp"
#include "utils/timing_stats.hpp"

namespace ebus {

/**
 * BusMonitor centralizes timing and performance statistics for the bus.
 * By separating telemetry from protocol logic, we keep the core FSMs clean.
 */
class BusMonitor {
 public:
  BusMonitor();
  void resetMetrics();
  metrics::SystemMetrics getMetrics() const;

  void updateUtilizationHistory();
  std::vector<float> getUtilizationHistory() const;

  // Thread-safe update helpers
  template <typename F>
  void updateHandler(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    updater(handler_acc_);
  }

  template <typename F>
  void updateRequest(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    updater(request_acc_);
  }

  template <typename F>
  void updateBus(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    updater(bus_acc_);
  }

  template <typename F>
  void updateDevice(F&& updater) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    updater(device_acc_);
  }

  // Accumulators
  mutable std::mutex metrics_mutex;

  TimingStats sync;
  TimingStats write;
  TimingStats passive_first;
  TimingStats passive_data;
  TimingStats active_first;
  TimingStats active_data;
  TimingStats callback_won;
  TimingStats callback_lost;
  TimingStats callback_reactive;
  TimingStats callback_telegram;
  TimingStats callback_error;

  // Physical Layer stats (moved from BusPosix/BusFreeRtos)
  TimingStats delay;
  TimingStats window;
  TimingStats transmit;
  TimingStats uptime;
  TimingStats syn_postpone;
  RollingStats utilization;

  detail::CircularBuffer<float> utilization_history_;

  // State-machine execution timings
  std::array<TimingStats, num_handler_states> handler_timing = {};

 private:
  metrics::HandlerMetrics handler_acc_;
  metrics::RequestMetrics request_acc_;
  metrics::BusMetrics bus_acc_;
  metrics::DeviceMetrics device_acc_;
};

}  // namespace ebus
