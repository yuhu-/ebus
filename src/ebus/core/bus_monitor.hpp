/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <mutex>

#include "ebus/definitions.hpp"
#include "ebus/metrics.hpp"
#include "utils/timing_stats.hpp"

namespace ebus {

/**
 * BusMonitor centralizes timing and performance statistics for the bus.
 * By separating telemetry from protocol logic, we keep the core FSMs clean.
 */
class BusMonitor {
 public:
  void resetMetrics();
  metrics::SystemMetrics getMetrics() const;

  void updateUtilizationHistory();
  std::vector<float> getUtilizationHistory() const;

  // Thread-safe update helpers
  void updateHandler(std::function<void(metrics::HandlerMetrics&)> updater);
  void updateRequest(std::function<void(metrics::RequestMetrics&)> updater);
  void updateBus(std::function<void(metrics::BusMetrics&)> updater);
  void updateDevice(std::function<void(metrics::DeviceMetrics&)> updater);

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

  static constexpr size_t MAX_HISTORY = 60;  // 1 minute at 1Hz
  std::array<float, MAX_HISTORY> utilization_history_ = {};
  size_t history_index_ = 0;
  size_t history_count_ = 0;

  // State-machine execution timings
  std::array<TimingStats, NUM_HANDLER_STATES> handler_timing = {};

 private:
  metrics::HandlerMetrics handler_acc_;
  metrics::RequestMetrics request_acc_;
  metrics::BusMetrics bus_acc_;
  metrics::DeviceMetrics device_acc_;
};

}  // namespace ebus
