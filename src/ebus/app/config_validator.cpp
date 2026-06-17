/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/address.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/json_reader.hpp>
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
  if (r.scheduler.max_items == 0) return false;

  // Ensure total timeout allows for at least one full FSM cycle plus overhead
  if (r.scheduler.total_timeout_ms <= r.scheduler.fsm_timeout_ms) return false;
  if (r.scheduler.total_timeout_ms <
      (r.scheduler.fsm_timeout_ms + r.scheduler.base_backoff_ms))
    return false;

  // 4. Network & Logging
  if (r.network.outbound_buffer_size == 0) return false;
  if (r.network.session_timeout_ms == 0) return false;
  if (r.network.transmit_timeout_ms == 0) return false;
  if (r.network.enable_server) {
    if (r.network.port_regular == 0 || r.network.port_readonly == 0 ||
        r.network.port_enhanced == 0)
      return false;
    if (r.network.port_regular == r.network.port_readonly ||
        r.network.port_regular == r.network.port_enhanced ||
        r.network.port_readonly == r.network.port_enhanced)
      return false;
  }
  if (r.poll.max_items == 0) return false;
  if (r.diagnostics.log_size > DiagnosticsLimits::log_history_size)
    return false;  // Sanity check

  // 5. Platform Specifics
#if defined(POSIX) && !EBUS_SIMULATION
  if (config.bus.device.empty()) return false;
#endif

  return true;
}

bool ConfigValidator::validateJson(std::string_view json) {
  if (!JsonReader::validate(json)) return false;
  JsonReader reader(json);

  // Check root fields
  auto address_token = reader.get("address");
  if (address_token == JsonReader::Token::number ||
      address_token == JsonReader::Token::string) {
    // Optimization Review: Use toNumStrict to reject illegal characters in
    // hex/dec strings
    auto val = reader.asNumStrict<int>();
    if (!val || !ebus::isMaster(static_cast<uint8_t>(*val))) return false;
  }

  if (reader.get("lock_counter") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<int>();
    if (!val || *val > RequestLimits::lock_counter_max) return false;
  }

  // Check nested bus fields
  auto window_token = reader.get("bus.window_us");
  if (window_token == JsonReader::Token::number ||
      window_token == JsonReader::Token::string) {
    auto val = reader.asNumStrict<int>();
    if (!val || *val < BusLimits::window_min_us ||
        *val > BusLimits::window_max_us)
      return false;
  }

  auto offset_token = reader.get("bus.offset_us");
  if (offset_token == JsonReader::Token::number ||
      offset_token == JsonReader::Token::string) {
    auto val = reader.asNumStrict<int>();
    if (!val || *val > BusLimits::offset_max_us) return false;
  }

  if (reader.get("bus.watchdog_timeout_ms") == JsonReader::Token::number) {
    if (reader.asNum<int>() == 0) return false;
  }

  // Check nested scheduler fields
  if (reader.get("scheduler.max_send_attempts") == JsonReader::Token::number) {
    int max_attempts = reader.asNum<int>();
    if (max_attempts < 1) return false;
  }

  uint32_t backoff = 100;
  if (reader.get("scheduler.base_backoff_ms") == JsonReader::Token::number) {
    backoff = reader.asNum<uint32_t>();
    if (backoff == 0) return false;
  }

  uint32_t fsm_timeout = 1000;
  if (reader.get("scheduler.fsm_timeout_ms") == JsonReader::Token::number) {
    fsm_timeout = reader.asNum<uint32_t>();
    if (fsm_timeout == 0) return false;
  }

  if (reader.get("scheduler.max_items") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<size_t>();
    if (!val || *val == 0) return false;
  }

  if (reader.get("scheduler.total_timeout_ms") == JsonReader::Token::number) {
    uint32_t total_timeout = reader.asNum<uint32_t>();
    // Ensure total timeout allows for at least one full cycle + backoff
    if (total_timeout <= fsm_timeout) return false;
    if (total_timeout < (fsm_timeout + backoff)) return false;
  }

  // Check nested network fields (Parity with struct validate)
  if (reader.get("network.outbound_buffer_size") == JsonReader::Token::number) {
    if (reader.asNum<size_t>() == 0) return false;
  }

  if (reader.get("network.session_timeout_ms") == JsonReader::Token::number) {
    if (reader.asNum<uint32_t>() == 0) return false;
  }

  if (reader.get("network.transmit_timeout_ms") == JsonReader::Token::number) {
    if (reader.asNum<uint32_t>() == 0) return false;
  }

  if (reader.get("network.port_regular") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<uint16_t>();
    if (!val || *val == 0) return false;
  }

  if (reader.get("network.port_readonly") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<uint16_t>();
    if (!val || *val == 0) return false;
  }

  if (reader.get("network.port_enhanced") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<uint16_t>();
    if (!val || *val == 0) return false;
  }

  if (reader.get("poll.max_items") == JsonReader::Token::number) {
    if (reader.asNum<size_t>() == 0) return false;
  }

  if (reader.get("diagnostics.log_size") == JsonReader::Token::number) {
    auto val = reader.asNumStrict<size_t>();
    if (!val || *val > DiagnosticsLimits::log_history_size) return false;
  }

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
