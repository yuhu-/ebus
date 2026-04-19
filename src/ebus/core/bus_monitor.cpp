/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

void ebus::BusMonitor::reset() {
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

void ebus::BusMonitor::updateHandlerMetrics(metrics::HandlerMetrics& m) const {
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
}

void ebus::BusMonitor::updateBusMetrics(metrics::BusMetrics& m) const {
  m.delay = delay.getValues();
  m.window = window.getValues();
  m.transmit = transmit.getValues();
  m.uptime = uptime.getValues();

  if (m.uptime.last > 0) {
    m.utilization = (utilization.getSum() / m.uptime.last) * 100.0;
  } else {
    m.utilization = 0.0;
  }
}
