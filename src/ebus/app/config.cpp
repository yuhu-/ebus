/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/config.hpp>
#include <string_view>

#include "utils/json_utils.hpp"

namespace ebus {

void RuntimeConfig::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "address", static_cast<int64_t>(address), first_field);
  append_field(json, "lock_counter", static_cast<int64_t>(lock_counter),
               first_field);
  append_field(json, "system_inquiry", system_inquiry, first_field);
  append_field(json, "system_response", system_response, first_field);

  json += ",\"bus\":{";
  bool bus_first_field = true;
  append_field(json, "window_us", static_cast<int64_t>(bus.window_us),
               bus_first_field);
  append_field(json, "offset_us", static_cast<int64_t>(bus.offset_us),
               bus_first_field);
  append_field(json, "watchdog_timeout_ms",
               static_cast<uint64_t>(bus.watchdog_timeout_ms), bus_first_field);
  append_field(json, "syn_gen", bus.syn_gen, bus_first_field);
  json += "}";

  json += ",\"diagnostics\":{";
  bool diag_first_field = true;
  append_enum_field(json, "level", diagnostics.level, diag_first_field);
  append_field(json, "log_size", static_cast<uint64_t>(diagnostics.log_size),
               diag_first_field);
  json += "}";

  json += ",\"network\":{";
  bool net_first_field = true;
  append_field(json, "session_timeout_ms",
               static_cast<uint64_t>(network.session_timeout_ms),
               net_first_field);
  append_field(json, "transmit_timeout_ms",
               static_cast<uint64_t>(network.transmit_timeout_ms),
               net_first_field);
  append_field(json, "outbound_buffer_size",
               static_cast<uint64_t>(network.outbound_buffer_size),
               net_first_field);
  json += "}";

  json += ",\"device\":{";
  bool dev_first_field = true;
  append_field(json, "scan_on_startup", device.scan_on_startup,
               dev_first_field);
  append_field(json, "initial_delay_s",
               static_cast<uint64_t>(device.initial_delay_s), dev_first_field);
  append_field(json, "startup_interval_s",
               static_cast<uint64_t>(device.startup_interval_s),
               dev_first_field);
  append_field(json, "max_startup_scans",
               static_cast<int64_t>(device.max_startup_scans), dev_first_field);
  json += "}";

  json += ",\"scheduler\":{";
  bool sched_first_field = true;
  append_field(json, "max_send_attempts",
               static_cast<int64_t>(scheduler.max_send_attempts),
               sched_first_field);
  append_field(json, "base_backoff_ms",
               static_cast<uint64_t>(scheduler.base_backoff_ms),
               sched_first_field);
  append_field(json, "fsm_timeout_ms",
               static_cast<uint64_t>(scheduler.fsm_timeout_ms),
               sched_first_field);
  append_field(json, "total_timeout_ms",
               static_cast<uint64_t>(scheduler.total_timeout_ms),
               sched_first_field);
  json += "}}";
}

RuntimeConfig RuntimeConfig::fromJson(const std::string& json) {
  RuntimeConfig cfg;
  if (!isValidJson(json)) {
    // Return a default-constructed config if the JSON is invalid
    return RuntimeConfig{};
  }
  std::string_view j = json;
  cfg.address = static_cast<uint8_t>(toNum<int>(extract(j, "address")));
  cfg.lock_counter =
      static_cast<uint8_t>(toNum<int>(extract(j, "lock_counter")));
  cfg.system_inquiry = extract(j, "system_inquiry") == "true";
  cfg.system_response = extract(j, "system_response") == "true";
  auto bus_j = extractSub(j, "bus");
  if (!bus_j.empty()) {
    cfg.bus.window_us =
        static_cast<uint16_t>(toNum<int>(extract(bus_j, "window_us")));
    cfg.bus.offset_us =
        static_cast<uint16_t>(toNum<int>(extract(bus_j, "offset_us")));
    cfg.bus.watchdog_timeout_ms =
        toNum<uint32_t>(extract(bus_j, "watchdog_timeout_ms"));
    cfg.bus.syn_gen = extract(bus_j, "syn_gen") == "true";
  }
  auto diag_j = extractSub(j, "diagnostics");
  if (!diag_j.empty()) {
    cfg.diagnostics.level =
        static_cast<LogLevel>(toNum<int>(extract(diag_j, "level")));
    cfg.diagnostics.log_size = toNum<size_t>(extract(diag_j, "log_size"));
  }
  auto net_j = extractSub(j, "network");
  if (!net_j.empty()) {
    cfg.network.session_timeout_ms =
        toNum<uint32_t>(extract(net_j, "session_timeout_ms"));
    cfg.network.transmit_timeout_ms =
        toNum<uint32_t>(extract(net_j, "transmit_timeout_ms"));
    cfg.network.outbound_buffer_size =
        toNum<size_t>(extract(net_j, "outbound_buffer_size"));
  }
  auto device_j = extractSub(j, "device");
  if (!device_j.empty()) {
    cfg.device.scan_on_startup = extract(device_j, "scan_on_startup") == "true";
    cfg.device.initial_delay_s =
        toNum<uint32_t>(extract(device_j, "initial_delay_s"));
    cfg.device.startup_interval_s =
        toNum<uint32_t>(extract(device_j, "startup_interval_s"));
    cfg.device.max_startup_scans = static_cast<uint8_t>(
        toNum<int>(extract(device_j, "max_startup_scans")));
  }
  auto sched_j = extractSub(j, "scheduler");
  if (!sched_j.empty()) {
    cfg.scheduler.max_send_attempts =
        static_cast<uint8_t>(toNum<int>(extract(sched_j, "max_send_attempts")));
    cfg.scheduler.base_backoff_ms =
        toNum<uint32_t>(extract(sched_j, "base_backoff_ms"));
    cfg.scheduler.fsm_timeout_ms =
        toNum<uint32_t>(extract(sched_j, "fsm_timeout_ms"));
    cfg.scheduler.total_timeout_ms =
        toNum<uint32_t>(extract(sched_j, "total_timeout_ms"));
  }
  return cfg;
}

bool RuntimeConfig::isValidJson(const std::string& json) {
  std::string_view sv = json;
  if (sv.empty()) return false;
  // Trim whitespace
  size_t first = sv.find_first_not_of(" \t\n\r");
  size_t last = sv.find_last_not_of(" \t\n\r");
  if (first == std::string::npos || last == std::string::npos)
    return false;  // All whitespace

  std::string_view trimmed_json = sv.substr(first, last - first + 1);

  if (trimmed_json.empty() || trimmed_json.front() != '{' ||
      trimmed_json.back() != '}') {
    return false;
  }

  // Basic check for balanced braces/brackets and string escaping
  int brace_count = 0;
  int bracket_count = 0;
  bool in_string = false;
  for (size_t i = 0; i < trimmed_json.length(); ++i) {
    char c = trimmed_json[i];
    if (c == '\\') {  // Skip escaped characters
      i++;
    } else if (c == '"') {
      in_string = !in_string;
    } else if (!in_string) {
      if (c == '{')
        brace_count++;
      else if (c == '}')
        brace_count--;
      else if (c == '[')
        bracket_count++;
      else if (c == ']')
        bracket_count--;
    }
    if (brace_count < 0 || bracket_count < 0) return false;  // Unbalanced
  }
  return brace_count == 0 && bracket_count == 0 && !in_string;
}

void BusConfig::toJson(std::string& json) const {
  json += "{";
  [[maybe_unused]] bool first_field = true;

#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
  append_field(json, "platform", "esp32", first_field);
  append_field(json, "uart_port", static_cast<int64_t>(uart_port), first_field);
  append_field(json, "rx_pin", static_cast<int64_t>(rx_pin), first_field);
  append_field(json, "tx_pin", static_cast<int64_t>(tx_pin), first_field);
  append_field(json, "timer_group", static_cast<int64_t>(timer_group),
               first_field);
  append_field(json, "timer_idx", static_cast<int64_t>(timer_idx), first_field);
#elif defined(POSIX) && !EBUS_SIMULATION
  append_field(json, "platform", "posix", first_field);
  append_field(json, "device", device, first_field);
#endif
  json += "}";
}

void EbusConfig::toJson(std::string& json) const {
  json += "{\"runtime\":";
  runtime.toJson(json);
  json += ",\"bus_hardware\":";
  bus.toJson(json);
  json += "}";
}

}  // namespace ebus