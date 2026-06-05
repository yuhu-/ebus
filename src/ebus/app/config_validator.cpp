/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/address.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/protocol_limits.hpp>

namespace ebus::detail {

bool ConfigValidator::validate(const EbusConfig& config) {
  const auto& r = config.runtime;

  // 1. Addressing
  if (!ebus::isMaster(r.address)) return false;
  if (r.lock_counter > RequestLimits::lock_counter_max) return false;

  // 2. Bus Layer
  if (r.bus.window_us < BusLimits::window_min_us ||
      r.bus.window_us > BusLimits::window_max_us)
    return false;
  if (r.bus.offset_us > BusLimits::offset_max_us) return false;
  if (r.bus.watchdog_timeout_ms == 0) return false;

  // 3. Scheduler & Logic
  if (r.scheduler.max_send_attempts < 1) return false;
  if (r.scheduler.base_backoff_ms == 0) return false;
  if (r.scheduler.fsm_timeout_ms == 0) return false;

  // Ensure total timeout allows for at least one full FSM cycle plus overhead
  if (r.scheduler.total_timeout_ms <= r.scheduler.fsm_timeout_ms) return false;
  if (r.scheduler.total_timeout_ms <
      (r.scheduler.fsm_timeout_ms + r.scheduler.base_backoff_ms))
    return false;

  // 4. Network & Logging
  if (r.network.outbound_buffer_size == 0) return false;
  if (r.network.session_timeout_ms == 0) return false;
  if (r.network.transmit_timeout_ms == 0) return false;
  if (r.diagnostics.log_size > DiagnosticsLimits::log_history_size)
    return false;  // Sanity check

  // 5. Platform Specifics
#if defined(POSIX) && !EBUS_SIMULATION
  if (config.bus.device.empty()) return false;
#endif

  return true;
}

bool ConfigValidator::requiresHardwareRestart(
    [[maybe_unused]] const EbusConfig& old_cfg,
    [[maybe_unused]] const EbusConfig& new_cfg) {
#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
  return old_cfg.bus.uart_port != new_cfg.bus.uart_port ||
         old_cfg.bus.rx_pin != new_cfg.bus.rx_pin ||
         old_cfg.bus.tx_pin != new_cfg.bus.tx_pin;
#elif defined(POSIX) && !EBUS_SIMULATION
  return old_cfg.bus.device != new_cfg.bus.device;
#else
  return false;
#endif
}

}  // namespace ebus::detail
