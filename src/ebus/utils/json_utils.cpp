/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <charconv>
#include <ctime>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/data_types.hpp>
#include <ebus/device.hpp>
#include <ebus/metrics.hpp>
#include <ebus/status.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/bus_monitor.hpp"

namespace ebus {

namespace {
/**
 * Finds a key safely by ensuring it is wrapped in quotes and followed by a
 * colon. This prevents matching keys that are substrings of other keys.
 */
size_t findKey(std::string_view json, std::string_view key) {
  size_t pos = 0;
  while ((pos = json.find('"', pos)) != std::string_view::npos) {
    std::string_view sub = json.substr(pos + 1);
    if (sub.size() > key.size() && sub.compare(0, key.size(), key) == 0 &&
        sub[key.size()] == '"') {
      size_t colon = json.find(':', pos + key.size() + 2);
      if (colon != std::string_view::npos) return colon + 1;
    }
    pos++;
  }
  return std::string_view::npos;
}

std::string_view extract(std::string_view json, std::string_view key) {
  size_t pos = findKey(json, key);
  if (pos == std::string_view::npos) return {};

  size_t start = json.find_first_not_of(" \t\n\r", pos);
  if (start == std::string_view::npos) return {};

  size_t end;
  if (json[start] == '"') {
    start++;
    // Basic robustness: handle escaped quotes \" by looking ahead
    end = start;
    while ((end = json.find('"', end)) != std::string_view::npos) {
      if (json[end - 1] != '\\') break;
      end++;
    }
  } else {
    end = json.find_first_of(", \t\n\r}", start);
  }
  return (end == std::string_view::npos) ? json.substr(start)
                                         : json.substr(start, end - start);
}

std::string_view extractSub(std::string_view json, std::string_view key) {
  size_t pos = findKey(json, key);
  if (pos == std::string_view::npos) return {};

  size_t start = json.find('{', pos);
  if (start == std::string_view::npos) return {};
  int depth = 0;
  for (size_t i = start; i < json.size(); ++i) {
    if (json[i] == '{')
      depth++;
    else if (json[i] == '}')
      depth--;
    if (depth == 0) return json.substr(start, i - start + 1);
  }
  return "";
}

template <typename T>
T toNum(std::string_view s) {
  if (s.empty() || s == "null") return 0;
  T val = 0;
  std::from_chars(s.data(), s.data() + s.size(), val);
  return val;
}
}  // namespace

// --- config.hpp ---

std::string RuntimeConfig::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"address\": " << static_cast<int>(address) << ","
      << "\"lock_counter\": " << static_cast<int>(lock_counter) << ","
      << "\"system_inquiry\": " << (system_inquiry ? "true" : "false") << ","
      << "\"system_response\": " << (system_response ? "true" : "false") << ","
      << "\"bus\": {"
      << "\"window_us\": " << bus.window_us << ","
      << "\"offset_us\": " << bus.offset_us << ","
      << "\"watchdog_timeout_ms\": " << bus.watchdog_timeout_ms << ","
      << "\"syn_gen\": " << (bus.syn_gen ? "true" : "false") << "},"
      << "\"diagnostics\": {"
      << "\"level\": " << static_cast<int>(diagnostics.level) << ","
      << "\"log_size\": " << diagnostics.log_size << "},"
      << "\"network\": {"
      << "\"session_timeout_ms\": " << network.session_timeout_ms << ","
      << "\"transmit_timeout_ms\": " << network.transmit_timeout_ms << ","
      << "\"outbound_buffer_size\": " << network.outbound_buffer_size << "},"
      << "\"scanner\": {"
      << "\"scan_on_startup\": " << (scanner.scan_on_startup ? "true" : "false")
      << ","
      << "\"initial_delay_s\": " << scanner.initial_delay_s << ","
      << "\"startup_interval_s\": " << scanner.startup_interval_s << ","
      << "\"max_startup_scans\": "
      << static_cast<int>(scanner.max_startup_scans) << "},"
      << "\"scheduler\": {"
      << "\"max_send_attempts\": " << scheduler.max_send_attempts << ","
      << "\"base_backoff_ms\": " << scheduler.base_backoff_ms << ","
      << "\"fsm_timeout_ms\": " << scheduler.fsm_timeout_ms << ","
      << "\"total_timeout_ms\": " << scheduler.total_timeout_ms << "}}";

  return oss.str();
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
  auto scan_j = extractSub(j, "scanner");
  if (!scan_j.empty()) {
    cfg.scanner.scan_on_startup = extract(scan_j, "scan_on_startup") == "true";
    cfg.scanner.initial_delay_s =
        toNum<uint32_t>(extract(scan_j, "initial_delay_s"));
    cfg.scanner.startup_interval_s =
        toNum<uint32_t>(extract(scan_j, "startup_interval_s"));
    cfg.scanner.max_startup_scans =
        static_cast<uint8_t>(toNum<int>(extract(scan_j, "max_startup_scans")));
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

std::string BusConfig::toJson() const {
  std::ostringstream oss;
  oss << "{";

#if defined(ESP_PLATFORM)
  oss << "\"platform\": \"esp32\","
      << "\"uart_port\": " << static_cast<int>(uart_port) << ","
      << "\"rx_pin\": " << static_cast<int>(rx_pin) << ","
      << "\"tx_pin\": " << static_cast<int>(tx_pin) << ","
      << "\"timer_group\": " << static_cast<int>(timer_group) << ","
      << "\"timer_idx\": " << static_cast<int>(timer_idx);
#elif defined(POSIX)
  oss << "\"platform\": \"posix\","
      << "\"device\": \"" << escapeJson(device) << "\","
      << "\"simulate\": " << (simulate ? "true" : "false");
#endif

  oss << "}";
  return oss.str();
}

std::string EbusConfig::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"runtime\":" << runtime.toJson() << ","
      << "\"bus_hardware\":" << bus.toJson() << "}";
  return oss.str();
}

// --- callbacks.hpp ---

std::string TelegramInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << session_id << ","
      << "\"poll_id\":" << poll_id << ","
      << "\"retry_count\":" << retry_count << ","
      << "\"message_type\":\"" << toString(message_type) << "\","
      << "\"telegram_type\":\"" << toString(telegram_type) << "\","
      << "\"handler_state\":\"" << toString(handler_state) << "\","
      << "\"request_state\":\"" << toString(request_state) << "\","
      << "\"master\":\"" << toString(master_view) << "\","
      << "\"slave\":\"" << toString(slave_view) << "\"}";
  return oss.str();
}

std::string ErrorInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << session_id << ","
      << "\"poll_id\":" << poll_id << ","
      << "\"level\":\"" << toString(level) << "\","
      << "\"protocol_error\":\"" << toString(protocol_error) << "\","
      << "\"result\":\"" << toString(result) << "\","
      << "\"sequence_state\":\"" << toString(sequence_state) << "\","
      << "\"handler_state\":\"" << toString(handler_state) << "\","
      << "\"request_state\":\"" << toString(request_state) << "\","
      << "\"master\":\"" << toString(master_view) << "\","
      << "\"slave\":\"" << toString(slave_view) << "\","
      << "\"utilization\":" << std::fixed << std::setprecision(2) << utilization
      << "}";
  return oss.str();
}

std::string ReactiveInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << session_id << ","
      << "\"master\":\"" << ebus::toString(master_view) << "\","
      << "\"slave_response\":\"" << slave_response.toString() << "\"}";
  return oss.str();
}

std::string ResultInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << session_id << ","
      << "\"poll_id\":" << poll_id << ","
      << "\"success\":" << (success ? "true" : "false") << ","
      << "\"result\":\"" << toString(result) << "\","
      << "\"sequence_state\":\"" << toString(sequence_state) << "\","
      << "\"master\":\"" << toString(master_view) << "\","
      << "\"slave\":\"" << toString(slave_view) << "\"}";
  return oss.str();
}

std::string BusEventContext::toJson() const {
  std::ostringstream oss;
  // Convert steady_clock to system_clock (approximation for external logs)
  auto wall_time =
      std::chrono::system_clock::now() +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          timestamp - std::chrono::steady_clock::now());
  time_t t = std::chrono::system_clock::to_time_t(wall_time);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                wall_time.time_since_epoch())
                .count() %
            1000;
  struct tm tm_info;
  gmtime_r(&t, &tm_info);

  oss << "{"
      << "\"byte\":\"" << toString(byte) << "\","
      << "\"handler_state\":\"" << toString(handler_state) << "\","
      << "\"request_state\":\"" << toString(request_state) << "\","
      << "\"result\":\"" << toString(result) << "\","
      << "\"lock_counter\":" << static_cast<int>(lock_counter) << ","
      << "\"timestamp\":\"" << std::put_time(&tm_info, "%Y-%m-%dT%H:%M:%S")
      << "." << std::setw(3) << std::setfill('0') << ms << "Z\""
      << "}";
  return oss.str();
}

// --- device.hpp ---

std::string DeviceInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"slave_address\":\"" << toString(slave_address) << "\","
      << "\"manufacturer\":\"" << toString(manufacturer) << "\","
      << "\"manufacturer_name\":\"" << escapeJson(manufacturer_name) << "\","
      << "\"unit_id\":\"" << escapeJson(unit_id) << "\","
      << "\"software_version\":\"" << escapeJson(software_version) << "\","
      << "\"hardware_version\":\"" << escapeJson(hardware_version) << "\"";

  if (!vaillant.serial_number.empty()) {
    oss << ",\"vaillant\":{"
        << "\"serial_number\":\"" << vaillant.serial_number << "\","
        << "\"product_code\":\"" << vaillant.product_code << "\""
        << "}";
  }

  oss << "}";
  return oss.str();
}

// --- types.hpp ---

std::string HandlerTransition::toJson() const {
  std::ostringstream oss;
  time_t s = static_cast<time_t>(timestamp / 1000);
  struct tm tm_info;
  gmtime_r(&s, &tm_info);

  oss << "{"
      << "\"from\":\"" << toString(from) << "\","
      << "\"to\":\"" << toString(to) << "\","
      << "\"timestamp\":\"" << std::put_time(&tm_info, "%Y-%m-%dT%H:%M:%SZ")
      << "\""
      << "}";
  return oss.str();
}

std::string RequestTransition::toJson() const {
  std::ostringstream oss;
  time_t s = static_cast<time_t>(timestamp / 1000);
  struct tm tm_info;
  gmtime_r(&s, &tm_info);

  oss << "{"
      << "\"from\":\"" << toString(from) << "\","
      << "\"to\":\"" << toString(to) << "\","
      << "\"timestamp\":\"" << std::put_time(&tm_info, "%Y-%m-%dT%H:%M:%SZ")
      << "\""
      << "}";
  return oss.str();
}

std::string ErrorEntry::toJson() const {
  std::ostringstream oss;
  time_t t = static_cast<time_t>(timestamp / 1000);
  struct tm tm_info;
  gmtime_r(&t, &tm_info);

  oss << "{"
      << "\"session_id\":" << session_id << ","
      << "\"poll_id\":" << poll_id << ","
      << "\"level\":\"" << ebus::toString(level) << "\","
      << "\"protocol_error\":\"" << ebus::toString(protocol_error) << "\","
      << "\"result\":\"" << ebus::toString(result) << "\","
      << "\"sequence_state\":\"" << ebus::toString(sequence_state) << "\","
      << "\"handler_state\":\"" << ebus::toString(handler_state) << "\","
      << "\"request_state\":\"" << ebus::toString(request_state) << "\","
      << "\"master\":\"" << ebus::toString(ByteView(master, master_len))
      << "\","
      << "\"slave\":\"" << ebus::toString(ByteView(slave, slave_len)) << "\","
      << "\"utilization\":" << std::fixed << std::setprecision(2) << utilization
      << ","
      << "\"timestamp\":\"" << std::put_time(&tm_info, "%Y-%m-%dT%H:%M:%SZ")
      << "\""
      << "}";
  return oss.str();
}

std::string metrics::HandlerMetrics::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"error_rate\":" << error_rate
      << ",\"protocol_data_utilization_rate\":"
      << protocol_data_utilization_rate
      << ",\"messages_total\":" << messages_total
      << ",\"error_total\":" << error_total
      << ",\"error_passive\":" << error_passive
      << ",\"error_reactive\":" << error_reactive
      << ",\"error_active\":" << error_active
      << ",\"total_data_bytes_sent\":" << total_data_bytes_sent
      << ",\"total_protocol_bytes_sent\":" << total_protocol_bytes_sent
      << ",\"sync\":" << sync.toJson() << ",\"write\":" << write.toJson()
      << ",\"passive_first\":" << passive_first.toJson()
      << ",\"passive_data\":" << passive_data.toJson()
      << ",\"active_first\":" << active_first.toJson()
      << ",\"active_data\":" << active_data.toJson()
      << ",\"callback_won\":" << callback_won.toJson()
      << ",\"callback_lost\":" << callback_lost.toJson()
      << ",\"callback_reactive\":" << callback_reactive.toJson()
      << ",\"callback_telegram\":" << callback_telegram.toJson()
      << ",\"callback_error\":" << callback_error.toJson();

  oss << ",\"state_timings\":{";
  for (size_t i = 0; i < detail::FsmLimits::num_handler_states; ++i) {
    if (i > 0) oss << ",";
    oss << "\"" << toString(static_cast<HandlerState>(i))
        << "\":" << state_timings[i].toJson();
  }
  oss << "},";

  oss << "\"transition_history\": [";
  // Note: We need a way to iterate the history without copying it to a vector.
  // This will be handled in ServiceStatus::toJson via the Monitor access.
  oss << "]";

  oss << "}";
  return oss.str();
}

std::string metrics::RequestMetrics::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"contention_rate\":" << contention_rate
      << ",\"collision_rate\":" << collision_rate
      << ",\"won_total\":" << won_total << ",\"lost_total\":" << lost_total
      << ",\"first_syn\":" << first_syn << ",\"first_won\":" << first_won
      << ",\"first_retry\":" << first_retry << ",\"first_lost\":" << first_lost
      << ",\"first_error\":" << first_error << ",\"retry_syn\":" << retry_syn
      << ",\"retry_error\":" << retry_error << ",\"second_won\":" << second_won
      << ",\"second_lost\":" << second_lost
      << ",\"second_error\":" << second_error;

  oss << ",\"transition_history\": [";
  for (size_t i = 0; i < transition_history.size(); ++i) {
    if (i > 0) oss << ",";
    oss << transition_history[i].toJson();
  }
  oss << "]";

  oss << "}";
  return oss.str();
}

std::string metrics::BusMetrics::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"utilization\":" << utilization
      << ",\"start_bit_errors\":" << start_bit_errors
      << ",\"syn_postponed_count\":" << syn_postponed_count;
  oss << ",\"congestion\":" << (congestion ? "true" : "false")
      << ",\"high_jitter\":" << (high_jitter ? "true" : "false");

  if (last_error_timestamp > 0) {
    time_t t = static_cast<time_t>(last_error_timestamp / 1000);
    struct tm tm_info;
    gmtime_r(&t, &tm_info);

    oss << ",\"last_error_timestamp\":\""
        << std::put_time(&tm_info, "%Y-%m-%dT%H:%M:%SZ") << "\"";
  }

  oss << ",\"delay\":" << delay.toJson() << ",\"window\":" << window.toJson()
      << ",\"transmit\":" << transmit.toJson()
      << ",\"uptime\":" << uptime.toJson()
      << ",\"syn_postpone\":" << syn_postpone.toJson() << "}";
  return oss.str();
}

std::string metrics::DeviceMetrics::toJson() const {
  std::ostringstream oss;
  oss << "{\"unknown_devices\":" << unknown_devices << ",\"masters\":{";
  bool first = true;
  for (size_t i = 0; i < 256; ++i) {
    if (masters[i] > 0) {
      if (!first) oss << ",";
      static constexpr char hex[] = "0123456789abcdef";
      oss << "\"0x" << hex[i >> 4] << hex[i & 0xf] << "\":" << masters[i];
      first = false;
    }
  }
  oss << "},\"slaves\":{";
  first = true;
  for (size_t i = 0; i < 256; ++i) {
    if (slaves[i] > 0) {
      if (!first) oss << ",";
      static constexpr char hex[] = "0123456789abcdef";
      oss << "\"0x" << hex[i >> 4] << hex[i & 0xf] << "\":" << slaves[i];
      first = false;
    }
  }
  oss << "}}";
  return oss.str();
}

std::string metrics::ControllerMetrics::toJson() const {
  std::ostringstream oss;
  oss << "{\"public_queue_dropped\":" << public_queue_dropped << "}";
  return oss.str();
}

std::string metrics::SystemMetrics::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"handler\":" << handler.toJson()
      << ",\"request\":" << request.toJson() << ",\"bus\":" << bus.toJson()
      << ",\"devices\":" << devices.toJson()
      << ",\"controller\":" << controller.toJson() << ",\"quality\":" << quality
      << "}";
  return oss.str();
}

// --- metrics.hpp ---

std::string MetricValues::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"last\":" << last << ",\"min\":" << min << ",\"max\":" << max
      << ",\"mean\":" << mean << ",\"std_dev\":" << stddev
      << ",\"count\":" << count << "}";
  return oss.str();
}

namespace {
std::string threadToJson(const ThreadStatus& s) {
  std::ostringstream oss;
  oss << "{"
      << "\"name\":\"" << (s.name.empty() ? "unknown" : escapeJson(s.name))
      << "\","
      << "\"stack_size\":" << s.task_stack_bytes << ","
      << "\"stack_free\":" << s.task_stack_free_bytes << "}";
  return oss.str();
}

std::string queueToJson(const ThreadStatus& thread, size_t size,
                        size_t capacity) {
  std::ostringstream oss;
  oss << "{"
      << "\"thread\":" << thread.toJson() << ","
      << "\"queue_size\":" << size << ","
      << "\"queue_capacity\":" << capacity << "}";
  return oss.str();
}
}  // namespace

std::string ThreadStatus::toJson() const { return threadToJson(*this); }

std::string ControllerStatus::toJson() const {
  return "{\"thread\":" + thread.toJson() + "}";
}

std::string BusStatus::toJson() const {
  return "{\"bus_thread\":" + bus_thread.toJson() +
         ",\"syn_thread\":" + syn_thread.toJson() + "}";
}

std::string BusHandlerStatus::toJson() const {
  return queueToJson(thread, queue_size, queue_capacity);
}

std::string SchedulerStatus::toJson() const {
  return queueToJson(thread, queue_size, queue_capacity);
}

std::string ClientManagerStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"thread\":" << thread.toJson() << ","
      << "\"queue_size\":" << queue_size << ","
      << "\"queue_capacity\":" << queue_capacity << ","
      << "\"session_active\":" << (session_active ? "true" : "false")
      << ",\"session_state\":\"" << escapeJson(session_state) << "\","
      << "\"last_error\":\"" << escapeJson(last_error) << "\"}";
  return oss.str();
}

std::string DeviceManagerStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"identified_count\":" << identified_count << ","
      << "\"unknown_count\":" << unknown_count << "}";
  return oss.str();
}

std::string DeviceScannerStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"is_scanning\":" << (is_scanning ? "true" : "false") << ","
      << "\"full_scan_active\":" << (full_scan_active ? "true" : "false") << ","
      << "\"full_scan_address\":" << full_scan_address << ","
      << "\"scan_on_startup_enabled\":"
      << (scan_on_startup_enabled ? "true" : "false") << ","
      << "\"startup_scan_count\":" << static_cast<int>(startup_scan_count)
      << ","
      << "\"manual_queue_size\":" << manual_queue_size << ","
      << "\"startup_queue_size\":" << startup_queue_size << "}";
  return oss.str();
}

std::string PollManagerStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"item_count\":" << item_count << "}";
  return oss.str();
}

std::string SystemResources::QueueInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"name\":\"" << escapeJson(name) << "\","
      << "\"size\":" << size << ","
      << "\"capacity\":" << capacity << "}";
  return oss.str();
}

std::string SystemResources::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"timestamp_ms\":" << timestamp_ms << ","
      << "\"threads\":[";
  for (size_t i = 0; i < threads.size(); ++i) {
    if (i > 0) oss << ",";
    oss << threads[i].toJson();
  }
  oss << "],\"queues\":[";
  for (size_t i = 0; i < queues.size(); ++i) {
    if (i > 0) oss << ",";
    oss << queues[i].toJson();
  }
  oss << "]}";
  return oss.str();
}

std::string ServiceStatus::toJson() const {
  return serializeServiceStatus(*this);
}

/**
 * To implement the zero-allocation requirement, we would need to pass
 * the actual BusMonitor to this function, as ServiceStatus no longer
 * holds the history vectors.
 */
std::string serializeServiceStatus(const ServiceStatus& status,
                                   detail::BusMonitor* monitor,
                                   bool reset_histories) {
  std::ostringstream oss;
  oss << "{"
      << "\"last_update_timestamp_ms\":" << status.last_update_timestamp_ms
      << ",\"controller\":" << status.controller.toJson()
      << ",\"bus\":" << status.bus.toJson()
      << ",\"bus_handler\":" << status.bus_handler.toJson()
      << ",\"scheduler\":" << status.scheduler.toJson()
      << ",\"client_manager\":" << status.client_manager.toJson()
      << ",\"device_manager\":" << status.device_manager.toJson()
      << ",\"device_scanner\":" << status.device_scanner.toJson()
      << ",\"poll_manager\":" << status.poll_manager.toJson();

  if (monitor) {
    oss << ",\"handler_history\":[";
    bool first = true;
    monitor->handler_history_.forEach([&](const HandlerTransition& t) {
      if (!first) oss << ",";
      oss << t.toJson();
      first = false;
    });
    oss << "],\"request_history\":[";
    first = true;
    monitor->request_history_.forEach([&](const RequestTransition& t) {
      if (!first) oss << ",";
      oss << t.toJson();
      first = false;
    });
    oss << "]";

    if (reset_histories) {
      monitor->handler_history_.clear();
      monitor->request_history_.clear();
      monitor->utilization_history_.clear();
    }
  }

  oss << "}";
  return oss.str();
}

// --- data_types.hpp ---

std::string DataTypeInfo::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"type\":" << static_cast<int32_t>(dt) << ","
      << "\"name\":\"" << name << "\","
      << "\"size\":" << static_cast<int>(size) << ","
      << "\"is_numeric\":" << (is_numeric ? "true" : "false") << ","
      << "\"is_signed\":" << (is_signed ? "true" : "false") << ","
      << "\"is_float\":" << (is_float ? "true" : "false") << ","
      << "\"has_replacement\":" << (has_replacement ? "true" : "false") << ","
      << "\"replacement_value\":" << replacement_value << ","
      << "\"factor\":" << std::fixed << std::setprecision(4) << factor << "}";
  return oss.str();
}

}  // namespace ebus
