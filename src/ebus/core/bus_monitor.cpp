/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

namespace ebus::detail {

BusMonitor::BusMonitor() : utilization_history_(LoggingLimits::history_size) {}

void BusMonitor::resetMetrics() {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  handler_acc_.resetMetrics();
  request_acc_.resetMetrics();
  bus_acc_.resetMetrics();
  device_acc_.resetMetrics();

  sync.reset();
  write.reset();
  passive_first.reset();
  passive_data.reset();
  active_first.reset();
  active_data.reset();
  callback_won.reset();
  callback_lost.reset();
  callback_reactive.reset();
  callback_telegram.reset();
  callback_error.reset();
  syn_postpone.reset();

  delay.reset();
  window.reset();
  transmit.reset();
  uptime.reset();
  utilization.reset();

  for (auto& stat : handler_timing) {
    stat.reset();
  }
}

ebus::metrics::SystemMetrics BusMonitor::getMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics::SystemMetrics sm;

  // 1. Populate Handler Part
  metrics::HandlerMetrics& hm = sm.handler;
  hm = handler_acc_;

  // Map Timing
  hm.sync = sync.getValues();
  hm.write = write.getValues();
  hm.passive_first = passive_first.getValues();
  hm.passive_data = passive_data.getValues();
  hm.active_first = active_first.getValues();
  hm.active_data = active_data.getValues();
  hm.callback_won = callback_won.getValues();
  hm.callback_lost = callback_lost.getValues();
  hm.callback_reactive = callback_reactive.getValues();
  hm.callback_telegram = callback_telegram.getValues();
  hm.callback_error = callback_error.getValues();

  for (size_t i = 0; i < FsmLimits::num_handler_states; ++i) {
    hm.state_timings[i] = handler_timing[i].getValues();
  }

  // Recalculate derived metrics
  hm.messages_total =
      hm.messages_passive_master_slave + hm.messages_passive_master_master +
      hm.messages_passive_broadcast + hm.messages_active_master_slave +
      hm.messages_active_master_master + hm.messages_active_broadcast +
      hm.messages_reactive_master_slave + hm.messages_reactive_master_master;

  hm.error_passive = hm.error_passive_master + hm.error_passive_master_ack +
                     hm.error_passive_slave + hm.error_passive_slave_ack;

  hm.error_reactive = hm.error_reactive_master + hm.error_reactive_master_ack +
                      hm.error_reactive_slave + hm.error_reactive_slave_ack;

  hm.error_active = hm.error_active_master + hm.error_active_master_ack +
                    hm.error_active_slave + hm.error_active_slave_ack;

  hm.error_total = hm.error_passive + hm.error_reactive + hm.error_active;

  //  Calculate Error Rate (%)
  if (hm.messages_total > 0) {
    hm.error_rate = (static_cast<float>(hm.error_total) /
                     (hm.messages_total + hm.error_total)) *
                    100.0f;
  }

  // Calculate Protocol Data Utilization Rate (%)
  if (hm.total_protocol_bytes_sent > 0) {
    hm.protocol_data_utilization_rate =
        (static_cast<float>(hm.total_data_bytes_sent) /
         hm.total_protocol_bytes_sent) *
        100.0f;
  }

  // 2. Populate Request Part
  metrics::RequestMetrics& rm = sm.request;
  rm = request_acc_;

  // Recalculate derived metrics
  rm.won_total = rm.first_won + rm.second_won;
  rm.lost_total = rm.first_lost + rm.second_lost;

  uint32_t attempts = rm.first_won + rm.first_lost + rm.first_retry;

  if (attempts > 0) {
    uint32_t collisions = rm.first_lost + rm.first_retry;
    // Calculate Contention Rate (%)
    rm.contention_rate = (static_cast<float>(collisions) / attempts) * 100.0f;
    // Calculate Collision Rate (%)
    rm.collision_rate = rm.contention_rate;
  }

  // 3. Populate Bus Part
  metrics::BusMetrics& bm = sm.bus;
  bm = bus_acc_;

  // Map Timing
  bm.delay = delay.getValues();
  bm.window = window.getValues();
  bm.transmit = transmit.getValues();
  bm.uptime = uptime.getValues();
  bm.syn_postpone = syn_postpone.getValues();

  // Physical Utilization (%)
  if (bm.uptime.last > 0) {
    bm.utilization = (utilization.getSum() / bm.uptime.last) * 100.0f;

    // Congestion Logic: > 70% for > 10 seconds
    // If a single uptime sample already indicates the bus has been up for
    // at least 10s (samples are in microseconds), treat high utilization
    // as sustained congestion immediately. Otherwise fall back to the
    // time-point based detection across successive calls.
    constexpr float kTenSecondsUs = 10e6f;  // 10 seconds in microseconds
    if (bm.utilization > 70.0f) {
      if (bm.uptime.last >= kTenSecondsUs) {
        congestion_active_ = true;
      } else {
        auto now = std::chrono::steady_clock::now();
        if (congestion_start_point_ ==
            std::chrono::steady_clock::time_point{}) {
          congestion_start_point_ = now;
        } else {
          auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                              now - congestion_start_point_)
                              .count();
          if (duration >= 10) {
            congestion_active_ = true;
          }
        }
      }
    } else {
      congestion_start_point_ = {};
      congestion_active_ = false;
    }
  }
  bm.congestion = congestion_active_;

  // High Jitter Logic: Standard deviation of SYN intervals > 10ms
  // (Standard eBUS unique timer tolerance is 5ms per Spec 9.2.2)
  bm.high_jitter = hm.sync.stddev > 10000.0f;

  // 4. Populate Device Part
  metrics::DeviceMetrics& dm = sm.devices;
  dm = device_acc_;

  // Calculate Quality Score (%)
  sm.quality = (100.0f - sm.handler.error_rate) *
               (1.0f - (sm.request.contention_rate / 100.0f));

  return sm;
}

void BusMonitor::updateUtilizationHistory() {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  float current_util = 0.0f;
  float up_last = uptime.getLast();
  if (up_last > 0.0f) {
    current_util = (utilization.getSum() / up_last) * 100.0f;
  }

  utilization_history_.push_back(static_cast<float>(current_util));
}

std::vector<float> BusMonitor::getUtilizationHistory() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  return utilization_history_.snapshot();
}

}  // namespace ebus::detail