/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ctime>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/device.hpp>
#include <ebus/metrics.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ebus {

/**
 * Escapes a string for use in a JSON value.
 */
inline std::string escapeJson(const std::string& s) {
  std::string res;
  res.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '"':
        res += "\\\"";
        break;
      case '\\':
        res += "\\\\";
        break;
      case '\b':
        res += "\\b";
        break;
      case '\f':
        res += "\\f";
        break;
      case '\n':
        res += "\\n";
        break;
      case '\r':
        res += "\\r";
        break;
      case '\t':
        res += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static const char hex[] = "0123456789abcdef";
          res += "\\u00";
          res += hex[(static_cast<unsigned char>(c) >> 4) & 0xf];
          res += hex[static_cast<unsigned char>(c) & 0xf];
        } else {
          res += c;
        }
    }
  }
  return res;
}

std::string toJson(const EbusConfig& config) {
  std::ostringstream oss;
  const auto& r = config.runtime;
  const auto& b = config.bus;

  oss << "{"
      << "\"runtime\":{"
      << "\"address\": " << static_cast<int>(r.address) << ","
      << "\"lock_counter\": " << static_cast<int>(r.lock_counter) << ","
      << "\"bus\": {"
      << "\"window_us\": " << r.bus.window_us << ","
      << "\"offset_us\": " << r.bus.offset_us << ","
      << "\"watchdog_timeout_ms\": " << r.bus.watchdog_timeout_ms << ","
      << "\"syn\": {"
      << "\"enabled\": " << (r.bus.syn.enabled ? "true" : "false") << ","
      << "\"base_ms\": " << r.bus.syn.base_ms << ","
      << "\"tolerance_ms\": " << r.bus.syn.tolerance_ms << "}},"
      << "\"logging\": {"
      << "\"level\": " << static_cast<int>(r.logging.level) << ","
      << "\"log_size\": " << r.logging.log_size << "},"
      << "\"network\": {"
      << "\"session_timeout_ms\": " << r.network.session_timeout_ms << ","
      << "\"transmit_timeout_ms\": " << r.network.transmit_timeout_ms << ","
      << "\"outbound_buffer_size\": " << r.network.outbound_buffer_size << "},"
      << "\"scanner\": {"
      << "\"scan_on_startup\": "
      << (r.scanner.scan_on_startup ? "true" : "false") << ","
      << "\"initial_delay_s\": " << r.scanner.initial_delay_s << ","
      << "\"startup_interval_s\": " << r.scanner.startup_interval_s << ","
      << "\"max_startup_scans\": "
      << static_cast<int>(r.scanner.max_startup_scans) << "},"
      << "\"scheduler\": {"
      << "\"max_send_attempts\": " << r.scheduler.max_send_attempts << ","
      << "\"base_backoff_ms\": " << r.scheduler.base_backoff_ms << ","
      << "\"fsm_timeout_ms\": " << r.scheduler.fsm_timeout_ms << ","
      << "\"total_timeout_ms\": " << r.scheduler.total_timeout_ms << "}},"
      << "\"bus_hardware\": {";

#if defined(ESP_PLATFORM)
  oss << "\"platform\": \"esp32\","
      << "\"uart_port\": " << static_cast<int>(b.uart_port) << ","
      << "\"rx_pin\": " << static_cast<int>(b.rx_pin) << ","
      << "\"tx_pin\": " << static_cast<int>(b.tx_pin) << ","
      << "\"timer_group\": " << static_cast<int>(b.timer_group) << ","
      << "\"timer_idx\": " << static_cast<int>(b.timer_idx);
#elif defined(POSIX)
  oss << "\"platform\": \"posix\","
      << "\"device\": \"" << escapeJson(b.device) << "\","
      << "\"simulate\": " << (b.simulate ? "true" : "false");
#endif

  oss << "}}";
  return oss.str();
}

std::string toJson(const ErrorInfo& info) {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << info.session_id << ","
      << "\"level\":" << static_cast<int>(info.level) << ","
      << "\"protocol_error\":\"" << toString(info.protocol_error) << "\","
      << "\"result\":\"" << toString(info.result) << "\","
      << "\"sequence_state\":" << static_cast<int>(info.sequence_state) << ","
      << "\"handler_state\":\"" << toString(info.handler_state) << "\","
      << "\"request_state\":\"" << toString(info.request_state) << "\","
      << "\"master\":\"" << toString(info.master_view) << "\","
      << "\"slave\":\"" << toString(info.slave_view) << "\","
      << "\"utilization\":" << std::fixed << std::setprecision(2)
      << info.utilization << "}";
  return oss.str();
}

std::string toJson(const ErrorEntry& entry) {
  std::ostringstream oss;
  time_t t = static_cast<time_t>(entry.timestamp / 1000);
  oss << "{"
      << "\"level\":" << static_cast<int>(entry.level) << ","
      << "\"protocol_error\":\"" << toString(entry.protocol_error) << "\","
      << "\"result\":\"" << toString(entry.result) << "\","
      << "\"sequence_state\":" << toString(entry.sequence_state) << ","
      << "\"handler_state\":\"" << toString(entry.handler_state) << "\","
      << "\"request_state\":\"" << toString(entry.request_state) << "\","
      << "\"master\":\"" << toString(ByteView(entry.master, entry.master_len))
      << "\","
      << "\"slave\":\"" << toString(ByteView(entry.slave, entry.slave_len))
      << "\","
      << "\"utilization\":" << std::fixed << std::setprecision(2)
      << entry.utilization << ","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

std::string toJson(const std::vector<ErrorEntry>& errors) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < errors.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(errors[i]);
  }
  oss << "]";
  return oss.str();
}

std::string toJson(const DeviceInfo& info) {
  std::ostringstream oss;
  oss << "{"
      << "\"slave_address\":" << static_cast<int>(info.slave_address) << ","
      << "\"manufacturer\":" << static_cast<int>(info.manufacturer) << ","
      << "\"manufacturer_name\":\"" << escapeJson(info.manufacturer_name)
      << "\","
      << "\"unit_id\":\"" << escapeJson(info.unit_id) << "\","
      << "\"software_version\":\"" << escapeJson(info.software_version) << "\","
      << "\"hardware_version\":\"" << escapeJson(info.hardware_version) << "\"";

  if (!info.vaillant.serial_number.empty()) {
    oss << ",\"vaillant\":{"
        << "\"serial_number\":\"" << info.vaillant.serial_number << "\","
        << "\"product_code\":\"" << info.vaillant.product_code << "\""
        << "}";
  }

  oss << "}";
  return oss.str();
}

std::string toJson(const std::vector<DeviceInfo>& devices) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < devices.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(devices[i]);
  }
  oss << "]";
  return oss.str();
}

std::string toJson(const MetricValues& v) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"last\":" << v.last << ",\"min\":" << v.min << ",\"max\":" << v.max
      << ",\"mean\":" << v.mean << ",\"stddev\":" << v.stddev
      << ",\"count\":" << v.count << "}";
  return oss.str();
}

std::string toJson(const BusEventContext& ctx) {
  std::ostringstream oss;
  // Convert steady_clock to system_clock (approximation for external logs)
  auto wall_time =
      std::chrono::system_clock::now() +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          ctx.timestamp - std::chrono::steady_clock::now());
  time_t t = std::chrono::system_clock::to_time_t(wall_time);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                wall_time.time_since_epoch())
                .count() %
            1000;

  oss << "{"
      << "\"byte\":\"" << toString(ctx.byte) << "\","
      << "\"handler_state\":\"" << toString(ctx.handler_state) << "\","
      << "\"request_state\":\"" << toString(ctx.request_state) << "\","
      << "\"result\":\"" << toString(ctx.result) << "\","
      << "\"lock_counter\":" << static_cast<int>(ctx.lock_counter) << ","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S") << "."
      << std::setw(3) << std::setfill('0') << ms << "Z\""
      << "}";
  return oss.str();
}

std::string toJson(const std::vector<BusEventContext>& trace) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < trace.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(trace[i]);
  }
  oss << "]";
  return oss.str();
}

std::string toJson(const HandlerTransition& t) {
  std::ostringstream oss;
  time_t s = static_cast<time_t>(t.timestamp / 1000);
  oss << "{"
      << "\"from\":\"" << toString(t.from) << "\","
      << "\"to\":\"" << toString(t.to) << "\","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&s), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

std::string toJson(const RequestTransition& t) {
  std::ostringstream oss;
  time_t s = static_cast<time_t>(t.timestamp / 1000);
  oss << "{"
      << "\"from\":\"" << toString(t.from) << "\","
      << "\"to\":\"" << toString(t.to) << "\","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&s), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

std::string toJson(const metrics::HandlerMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"error_rate\":" << m.error_rate
      << ",\"protocol_data_utilization_rate\":"
      << m.protocol_data_utilization_rate
      << ",\"messages_total\":" << m.messages_total
      << ",\"error_total\":" << m.error_total
      << ",\"error_passive\":" << m.error_passive
      << ",\"error_reactive\":" << m.error_reactive
      << ",\"error_active\":" << m.error_active
      << ",\"total_data_bytes_sent\":" << m.total_data_bytes_sent
      << ",\"total_protocol_bytes_sent\":" << m.total_protocol_bytes_sent
      << ",\"sync\":" << toJson(m.sync) << ",\"write\":" << toJson(m.write)
      << ",\"passive_first\":" << toJson(m.passive_first)
      << ",\"passive_data\":" << toJson(m.passive_data)
      << ",\"active_first\":" << toJson(m.active_first)
      << ",\"active_data\":" << toJson(m.active_data)
      << ",\"callback_won\":" << toJson(m.callback_won)
      << ",\"callback_lost\":" << toJson(m.callback_lost)
      << ",\"callback_reactive\":" << toJson(m.callback_reactive)
      << ",\"callback_telegram\":" << toJson(m.callback_telegram)
      << ",\"callback_error\":" << toJson(m.callback_error);

  oss << ",\"state_timings\":{";
  for (size_t i = 0; i < detail::FsmLimits::num_handler_states; ++i) {
    if (i > 0) oss << ",";
    oss << "\"" << toString(static_cast<HandlerState>(i))
        << "\":" << toJson(m.state_timings[i]);
  }
  oss << "},";

  oss << "\"transition_history\": [";
  for (size_t i = 0; i < m.transition_history.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(m.transition_history[i]);
  }
  oss << "]";

  oss << "}";
  return oss.str();
}

std::string toJson(const metrics::RequestMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"contention_rate\":" << m.contention_rate
      << ",\"collision_rate\":" << m.collision_rate
      << ",\"won_total\":" << m.won_total << ",\"lost_total\":" << m.lost_total
      << ",\"first_syn\":" << m.first_syn << ",\"first_won\":" << m.first_won
      << ",\"first_retry\":" << m.first_retry
      << ",\"first_lost\":" << m.first_lost
      << ",\"first_error\":" << m.first_error
      << ",\"retry_syn\":" << m.retry_syn
      << ",\"retry_error\":" << m.retry_error
      << ",\"second_won\":" << m.second_won
      << ",\"second_lost\":" << m.second_lost
      << ",\"second_error\":" << m.second_error;

  oss << ",\"transition_history\": [";
  for (size_t i = 0; i < m.transition_history.size(); ++i) {
    if (i > 0) oss << ",";
    oss << toJson(m.transition_history[i]);
  }
  oss << "]";

  oss << "}";
  return oss.str();
}

std::string toJson(const metrics::BusMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"utilization\":" << m.utilization
      << ",\"start_bit_errors\":" << m.start_bit_errors
      << ",\"syn_postponed_count\":" << m.syn_postponed_count;
  oss << ",\"congestion\":" << (m.congestion ? "true" : "false")
      << ",\"high_jitter\":" << (m.high_jitter ? "true" : "false");

  if (m.last_error_timestamp > 0) {
    time_t t = static_cast<time_t>(m.last_error_timestamp / 1000);
    oss << ",\"last_error_timestamp\":\""
        << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\"";
  }

  oss << ",\"delay\":" << toJson(m.delay) << ",\"window\":" << toJson(m.window)
      << ",\"transmit\":" << toJson(m.transmit)
      << ",\"uptime\":" << toJson(m.uptime)
      << ",\"syn_postpone\":" << toJson(m.syn_postpone) << "}";
  return oss.str();
}

std::string toJson(const metrics::DeviceMetrics& m) {
  std::ostringstream oss;
  oss << "{\"unknown_devices\":" << m.unknown_devices << ",\"masters\":{";
  bool first = true;
  for (size_t i = 0; i < 256; ++i) {
    if (m.masters[i] > 0) {
      if (!first) oss << ",";
      static constexpr char hex[] = "0123456789abcdef";
      oss << "\"0x" << hex[i >> 4] << hex[i & 0xf] << "\":" << m.masters[i];
      first = false;
    }
  }
  oss << "},\"slaves\":{";
  first = true;
  for (size_t i = 0; i < 256; ++i) {
    if (m.slaves[i] > 0) {
      if (!first) oss << ",";
      static constexpr char hex[] = "0123456789abcdef";
      oss << "\"0x" << hex[i >> 4] << hex[i & 0xf] << "\":" << m.slaves[i];
      first = false;
    }
  }
  oss << "}}";
  return oss.str();
}

std::string toJson(const metrics::SystemMetrics& sm) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"handler\":" << toJson(sm.handler)
      << ",\"request\":" << toJson(sm.request) << ",\"bus\":" << toJson(sm.bus)
      << ",\"devices\":" << toJson(sm.devices) << ",\"quality\":" << sm.quality
      << "}";
  return oss.str();
}

}  // namespace ebus
