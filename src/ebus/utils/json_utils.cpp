/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ctime>
#include <ebus/callbacks.hpp>
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

std::string toJson(const ErrorInfo& info) {
  std::ostringstream oss;
  oss << "{"
      << "\"session_id\":" << info.session_id << ","
      << "\"level\":" << static_cast<int>(info.level) << ","
      << "\"message\":\"" << escapeJson(std::string(info.message)) << "\","
      << "\"result\":\"" << toString(info.result) << "\","
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
  auto t = std::chrono::system_clock::to_time_t(entry.timestamp);
  oss << "{"
      << "\"level\":" << static_cast<int>(entry.level) << ","
      << "\"message\":\"" << escapeJson(entry.message) << "\","
      << "\"result\":\"" << toString(entry.result) << "\","
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
  oss << "}}";
  return oss.str();
}

std::string toJson(const metrics::RequestMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"contention_rate\":" << m.contention_rate
      << ",\"won_total\":" << m.won_total << ",\"lost_total\":" << m.lost_total
      << ",\"first_syn\":" << m.first_syn << ",\"first_won\":" << m.first_won
      << ",\"first_retry\":" << m.first_retry
      << ",\"first_lost\":" << m.first_lost
      << ",\"first_error\":" << m.first_error
      << ",\"retry_syn\":" << m.retry_syn
      << ",\"retry_error\":" << m.retry_error
      << ",\"second_won\":" << m.second_won
      << ",\"second_lost\":" << m.second_lost
      << ",\"second_error\":" << m.second_error << "}";
  return oss.str();
}

std::string toJson(const metrics::BusMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"utilization\":" << m.utilization
      << ",\"start_bit_errors\":" << m.start_bit_errors
      << ",\"syn_postponed_count\":" << m.syn_postponed_count;

  if (m.last_error_timestamp != std::chrono::system_clock::time_point{}) {
    auto t = std::chrono::system_clock::to_time_t(m.last_error_timestamp);
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
