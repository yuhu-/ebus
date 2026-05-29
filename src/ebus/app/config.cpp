/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/config.hpp>
#include <ebus/detail/json_writer.hpp>  // For detail::JsonWriter
#include <ebus/utils.hpp>
#include <string_view>

namespace ebus {

void RuntimeConfig::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("address", static_cast<uint64_t>(address));
  writer.writeField("lock_counter", static_cast<uint64_t>(lock_counter));
  writer.writeField("system_inquiry", system_inquiry);
  writer.writeField("system_response", system_response);

  writer.appendKey("bus");
  writer.startObject();
  writer.writeField("window_us", static_cast<uint64_t>(bus.window_us));
  writer.writeField("offset_us", static_cast<uint64_t>(bus.offset_us));
  writer.writeField("watchdog_timeout_ms",
                    static_cast<uint64_t>(bus.watchdog_timeout_ms));
  writer.writeField("syn_gen", bus.syn_gen);
  writer.endObject();

  writer.appendKey("diagnostics");
  writer.startObject();
  writer.writeField("level", toString(diagnostics.level));
  writer.writeField("log_size", static_cast<uint64_t>(diagnostics.log_size));
  writer.endObject();

  writer.appendKey("network");
  writer.startObject();
  writer.writeField("session_timeout_ms",
                    static_cast<uint64_t>(network.session_timeout_ms));
  writer.writeField("transmit_timeout_ms",
                    static_cast<uint64_t>(network.transmit_timeout_ms));
  writer.writeField("outbound_buffer_size",
                    static_cast<uint64_t>(network.outbound_buffer_size));
  writer.endObject();

  writer.appendKey("device");
  writer.startObject();
  writer.writeField("scan_on_startup", device.scan_on_startup);
  writer.writeField("initial_delay_s",
                    static_cast<uint64_t>(device.initial_delay_s));
  writer.writeField("startup_interval_s",
                    static_cast<uint64_t>(device.startup_interval_s));
  writer.writeField("max_startup_scans",
                    static_cast<uint64_t>(device.max_startup_scans));
  writer.endObject();

  writer.appendKey("scheduler");
  writer.startObject();
  writer.writeField("max_send_attempts",
                    static_cast<uint64_t>(scheduler.max_send_attempts));
  writer.writeField("base_backoff_ms",
                    static_cast<uint64_t>(scheduler.base_backoff_ms));
  writer.writeField("fsm_timeout_ms",
                    static_cast<uint64_t>(scheduler.fsm_timeout_ms));
  writer.writeField("total_timeout_ms",
                    static_cast<uint64_t>(scheduler.total_timeout_ms));
  writer.endObject();
  writer.endObject();
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

void BusConfig::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();

#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
  writer.writeField("platform", "esp32");
  writer.writeField("uart_port", static_cast<uint64_t>(uart_port));
  writer.writeField("rx_pin", static_cast<uint64_t>(rx_pin));
  writer.writeField("tx_pin", static_cast<uint64_t>(tx_pin));
  writer.writeField("timer_group", static_cast<uint64_t>(timer_group));
  writer.writeField("timer_idx", static_cast<uint64_t>(timer_idx));
#elif defined(POSIX) && !EBUS_SIMULATION
  writer.writeField("platform", "posix");
  writer.writeField("device", device);
#endif
  writer.endObject();
}

void EbusConfig::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("runtime");
  runtime.toJson(visitor);
  writer.appendKey("bus_hardware");
  bus.toJson(visitor);
  writer.endObject();
}

}  // namespace ebus