/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

void ebus::BusMonitor::reset() {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  handler_acc_.reset();
  request_acc_.reset();
  bus_acc_.reset();

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

  delay.reset();
  window.reset();
  transmit.reset();
  uptime.reset();
  utilization.reset();

  for (auto& stat : handler_timing) {
    stat.reset();
  }
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

ebus::metrics::HandlerMetrics ebus::BusMonitor::getHandlerMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics::HandlerMetrics m = handler_acc_;

  // Recalculate derived metrics
  m.messages_total =
      m.messages_passive_master_slave + m.messages_passive_master_master +
      m.messages_passive_broadcast + m.messages_active_master_slave +
      m.messages_active_master_master + m.messages_active_broadcast +
      m.messages_reactive_master_slave + m.messages_reactive_master_master;

  m.error_passive = m.error_passive_master + m.error_passive_master_ack +
                    m.error_passive_slave + m.error_passive_slave_ack;

  m.error_reactive = m.error_reactive_master + m.error_reactive_master_ack +
                     m.error_reactive_slave + m.error_reactive_slave_ack;

  m.error_active = m.error_active_master + m.error_active_master_ack +
                   m.error_active_slave + m.error_active_slave_ack;

  m.error_total = m.error_passive + m.error_reactive + m.error_active;

  //  Calculate Error Rate (%)
  if (m.messages_total > 0) {
    m.error_rate = (static_cast<double>(m.error_total) /
                    (m.messages_total + m.error_total)) *
                   100.0;
  }

  // Calculate Protocol Data Utilization Rate (%)
  if (m.total_protocol_bytes_sent > 0) {
    m.protocol_data_utilization_rate =
        (static_cast<double>(m.total_data_bytes_sent) /
         m.total_protocol_bytes_sent) *
        100.0;
  }

  // Map Timing
  m.sync = sync.getValues();
  m.write = write.getValues();
  m.passive_first = passive_first.getValues();
  m.passive_data = passive_data.getValues();
  m.active_first = active_first.getValues();
  m.active_data = active_data.getValues();
  m.callback_won = callback_won.getValues();
  m.callback_lost = callback_lost.getValues();
  m.callback_reactive = callback_reactive.getValues();
  m.callback_telegram = callback_telegram.getValues();
  m.callback_error = callback_error.getValues();

  for (size_t i = 0; i < NUM_HANDLER_STATES; ++i) {
    m.state_timings[i] = handler_timing[i].getValues();
  }
  return m;
}

ebus::metrics::RequestMetrics ebus::BusMonitor::getRequestMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics::RequestMetrics m = request_acc_;

  // Recalculate derived metrics
  m.won_total = m.first_won + m.second_won;
  m.lost_total = m.first_lost + m.second_lost;

  uint32_t attempts = m.first_won + m.first_lost + m.first_retry;

  // Calculate Contention Rate (%)
  if (attempts > 0) {
    uint32_t collisions = m.first_lost + m.first_retry;
    m.contention_rate = (static_cast<double>(collisions) / attempts) * 100.0;
  }
  return m;
}

ebus::metrics::BusMetrics ebus::BusMonitor::getBusMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex);
  metrics::BusMetrics m = bus_acc_;

  // Physical Utilization (%)
  if (m.uptime.last > 0) {
    m.utilization = (utilization.getSum() / m.uptime.last) * 100.0;
  }

  // Map Timing
  m.delay = delay.getValues();
  m.window = window.getValues();
  m.transmit = transmit.getValues();
  m.uptime = uptime.getValues();

  return m;
}
