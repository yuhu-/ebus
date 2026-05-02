/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ebus/config.hpp"
#include "ebus/detail/protocol_limits.hpp"
#include "ebus/types.hpp"

namespace ebus::detail {

/**
 * Validates EbusConfig against protocol limits and logical constraints.
 */
class ConfigValidator {
 public:
  static bool validate(const EbusConfig& config) {
    const auto& r = config.runtime;

    // 1. Addressing
    if (!ebus::isMaster(r.address) && r.address != 0xff) return false;
    if (r.lock_counter > RequestLimits::lock_counter_max) return false;

    // 2. Bus Layer
    if (r.bus.window_us < BusLimits::window_min_us ||
        r.bus.window_us > BusLimits::window_max_us)
      return false;
    if (r.bus.offset_us > BusLimits::offset_max_us) return false;
    if (r.bus.watchdog_timeout_ms == 0) return false;
    if (r.bus.syn.enabled && r.bus.syn.base_ms == 0) return false;

    // 3. Scheduler & Logic
    if (r.scheduler.max_send_attempts < 1) return false;
    if (r.scheduler.base_backoff_ms == 0) return false;
    if (r.scheduler.fsm_timeout_ms == 0) return false;

    // Ensure total timeout allows for at least one full FSM cycle plus overhead
    if (r.scheduler.total_timeout_ms <= r.scheduler.fsm_timeout_ms)
      return false;
    if (r.scheduler.total_timeout_ms <
        (r.scheduler.fsm_timeout_ms + r.scheduler.base_backoff_ms))
      return false;

    // 4. Network & Logging
    if (r.network.outbound_buffer_size == 0) return false;
    if (r.network.session_timeout_ms == 0) return false;
    if (r.network.transmit_timeout_ms == 0) return false;
    if (r.logging.log_size > LoggingLimits::history_size)
      return false;  // Sanity check

    // 5. Platform Specifics
#if defined(POSIX)
    if (config.bus.device.empty() && !config.bus.simulate) return false;
#endif

    return true;
  }

  /**
   * Checks if two configs require a hardware restart (Bus/UART recreation).
   */
  static bool requiresHardwareRestart(const EbusConfig& old_cfg,
                                      const EbusConfig& new_cfg) {
#if defined(ESP_PLATFORM)
    return old_cfg.bus.uart_port != new_cfg.bus.uart_port ||
           old_cfg.bus.rx_pin != new_cfg.bus.rx_pin ||
           old_cfg.bus.tx_pin != new_cfg.bus.tx_pin;
#elif defined(POSIX)
    return old_cfg.bus.device != new_cfg.bus.device ||
           old_cfg.bus.simulate != new_cfg.bus.simulate;
#else
    return false;
#endif
  }
};

}  // namespace ebus::detail
