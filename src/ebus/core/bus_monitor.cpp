/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

void ebus::BusMonitor::resetMetrics() {
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
  history_index_ = 0;
  history_count_ = 0;

  for (auto& stat : handler_timing) {
    stat.reset();
  }
}

ebus::metrics::SystemMetrics ebus::BusMonitor::getMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics::SystemMetrics sm;

  // 1. Populate Handler Part
  metrics::HandlerMetrics& hm = sm.handler;
  hm = handler_acc_;

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
    hm.error_rate = (static_cast<double>(hm.error_total) /
                     (hm.messages_total + hm.error_total)) *
                    100.0;
  }

  // Calculate Protocol Data Utilization Rate (%)
  if (hm.total_protocol_bytes_sent > 0) {
    hm.protocol_data_utilization_rate =
        (static_cast<double>(hm.total_data_bytes_sent) /
         hm.total_protocol_bytes_sent) *
        100.0;
  }

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

  for (size_t i = 0; i < num_handler_states; ++i) {
    hm.state_timings[i] = handler_timing[i].getValues();
  }

  // 2. Populate Request Part
  metrics::RequestMetrics& rm = sm.request;
  rm = request_acc_;

  // Recalculate derived metrics
  rm.won_total = rm.first_won + rm.second_won;
  rm.lost_total = rm.first_lost + rm.second_lost;

  uint32_t attempts = rm.first_won + rm.first_lost + rm.first_retry;

  // Calculate Contention Rate (%)
  if (attempts > 0) {
    uint32_t collisions = rm.first_lost + rm.first_retry;
    rm.contention_rate = (static_cast<double>(collisions) / attempts) * 100.0;
  }

  // 3. Populate Bus Part
  metrics::BusMetrics& bm = sm.bus;
  bm = bus_acc_;

  // Physical Utilization (%)
  if (bm.uptime.last > 0) {
    bm.utilization = (utilization.getSum() / bm.uptime.last) * 100.0;
  }

  // Map Timing
  bm.delay = delay.getValues();
  bm.window = window.getValues();
  bm.transmit = transmit.getValues();
  bm.uptime = uptime.getValues();
  bm.syn_postpone = syn_postpone.getValues();

  // 4. Populate Device Part
  metrics::DeviceMetrics& dm = sm.devices;
  dm = device_acc_;

  // Calculate Quality Score (%)
  sm.quality = (100.0 - sm.handler.error_rate) *
               (1.0 - (sm.request.contention_rate / 100.0));

  return sm;
}

void ebus::BusMonitor::updateUtilizationHistory() {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  double current_util = 0.0;
  if (bus_acc_.uptime.last > 0) {
    current_util = (utilization.getSum() / bus_acc_.uptime.last) * 100.0;
  }

  utilization_history_[history_index_] = static_cast<float>(current_util);
  history_index_ = (history_index_ + 1) % ebus::default_history_size;
  if (history_count_ < ebus::default_history_size) history_count_++;
}

std::vector<float> ebus::BusMonitor::getUtilizationHistory() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  std::vector<float> result;
  result.reserve(history_count_);

  for (size_t i = 0; i < history_count_; ++i) {
    size_t idx =
        (history_index_ + ebus::default_history_size - history_count_ + i) %
        ebus::default_history_size;
    result.push_back(utilization_history_[idx]);
  }
  return result;
}

void ebus::BusMonitor::updateHandler(
    std::function<void(metrics::HandlerMetrics&)> updater) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  updater(handler_acc_);
}

void ebus::BusMonitor::updateRequest(
    std::function<void(metrics::RequestMetrics&)> updater) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  updater(request_acc_);
}

void ebus::BusMonitor::updateBus(
    std::function<void(metrics::BusMetrics&)> updater) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  updater(bus_acc_);
}

void ebus::BusMonitor::updateDevice(
    std::function<void(metrics::DeviceMetrics&)> updater) {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  updater(device_acc_);
}
