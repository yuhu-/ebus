/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace ebus {

/**
 * Results of a rolling metric calculation.
 */
struct MetricValues {
  double last = 0.0;
  double min = 0.0;
  double max = 0.0;
  double mean = 0.0;
  double stddev = 0.0;
  uint64_t count = 0;
};

/**
 * Serializes MetricValues to a JSON object string.
 */
inline std::string toJson(const MetricValues& v) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"last\":" << v.last << ",\"min\":" << v.min << ",\"max\":" << v.max
      << ",\"mean\":" << v.mean << ",\"stddev\":" << v.stddev
      << ",\"count\":" << v.count << "}";
  return oss.str();
}

namespace metrics {

/**
 * Performance and health metrics for the protocol engine.
 */
struct HandlerMetrics {
  // Error Rate (%)
  // Error Rate is calculated as the total number of errors divided by the
  // total number of messages (including errors). This provides insight into the
  // reliability of the communication, with a lower error rate indicating better
  // performance.
  double error_rate = 0.0;

  // Protocol Data Utilization Rate (%)
  // This metric represents the efficiency of data transmission, calculated as
  // the ratio of protocol bytes (address + data) to total bytes sent on the bus
  // (including overhead like SYN, ACK, etc.). A higher utilization rate
  // indicates more efficient use of the bus bandwidth for actual data
  // transmission.
  double protocol_data_utilization_rate = 0.0;

  // Aggregated Counters
  uint32_t messages_total = 0;
  uint32_t error_total = 0;
  uint32_t error_passive = 0;
  uint32_t error_reactive = 0;
  uint32_t error_active = 0;
  uint32_t total_data_bytes_sent = 0;
  uint32_t total_protocol_bytes_sent = 0;

  // Detailed Counters
  uint32_t messages_passive_master_slave = 0;
  uint32_t messages_passive_master_master = 0;
  uint32_t messages_passive_broadcast = 0;
  uint32_t messages_reactive_master_slave = 0;
  uint32_t messages_reactive_master_master = 0;
  uint32_t messages_active_master_slave = 0;
  uint32_t messages_active_master_master = 0;
  uint32_t messages_active_broadcast = 0;
  uint32_t reset_passive_00 = 0;
  uint32_t reset_passive_0704 = 0;
  uint32_t reset_passive = 0;
  uint32_t reset_active_00 = 0;
  uint32_t reset_active_0704 = 0;
  uint32_t reset_active = 0;
  uint32_t error_passive_master = 0;
  uint32_t error_passive_master_ack = 0;
  uint32_t error_passive_slave = 0;
  uint32_t error_passive_slave_ack = 0;
  uint32_t error_reactive_master = 0;
  uint32_t error_reactive_master_ack = 0;
  uint32_t error_reactive_slave = 0;
  uint32_t error_reactive_slave_ack = 0;
  uint32_t error_active_master = 0;
  uint32_t error_active_master_ack = 0;
  uint32_t error_active_slave = 0;
  uint32_t error_active_slave_ack = 0;

  // Explicit phase timings
  MetricValues sync;
  MetricValues write;
  MetricValues passive_first;
  MetricValues passive_data;
  MetricValues active_first;
  MetricValues active_data;
  MetricValues callback_won;
  MetricValues callback_lost;
  MetricValues callback_reactive;
  MetricValues callback_telegram;
  MetricValues callback_error;

  // State-machine specific execution timings
  std::array<MetricValues, 15> state_timings;

  void resetMetrics() {
    error_rate = 0.0;
    protocol_data_utilization_rate = 0.0;

    // Aggregated Counters
    messages_total = 0;
    error_total = 0;
    error_passive = 0;
    error_reactive = 0;
    error_active = 0;
    total_data_bytes_sent = 0;
    total_protocol_bytes_sent = 0;

    // Detailed Counters
    messages_passive_master_slave = 0;
    messages_passive_master_master = 0;
    messages_passive_broadcast = 0;
    messages_reactive_master_slave = 0;
    messages_reactive_master_master = 0;
    messages_active_master_slave = 0;
    messages_active_master_master = 0;
    messages_active_broadcast = 0;
    reset_passive_00 = 0;
    reset_passive_0704 = 0;
    reset_passive = 0;
    reset_active_00 = 0;
    reset_active_0704 = 0;
    reset_active = 0;
    error_passive_master = 0;
    error_passive_master_ack = 0;
    error_passive_slave = 0;
    error_passive_slave_ack = 0;
    error_reactive_master = 0;
    error_reactive_master_ack = 0;
    error_reactive_slave = 0;
    error_reactive_slave_ack = 0;
    error_active_master = 0;
    error_active_master_ack = 0;
    error_active_slave = 0;
    error_active_slave_ack = 0;

    // Explicit phase timings
    sync = {};
    write = {};
    passive_first = {};
    passive_data = {};
    active_first = {};
    active_data = {};
    callback_won = {};
    callback_lost = {};
    callback_reactive = {};
    callback_telegram = {};
    callback_error = {};

    // State-machine specific execution timings
    state_timings.fill({});
  }
};

/**
 * Performance and health metrics for bus requests.
 */
struct RequestMetrics {
  // Contention Rate (%)
  // Contention happens when we lose arbitration (lost or retry) on the
  // first attempt. This is calculated as the number of contention events
  // divided by the total number of bus request attempts.
  double contention_rate = 0.0;

  // Aggregated Counters
  uint32_t won_total = 0;
  uint32_t lost_total = 0;

  // Detailed Counters
  uint32_t first_syn = 0;
  uint32_t first_won = 0;
  uint32_t first_retry = 0;
  uint32_t first_lost = 0;
  uint32_t first_error = 0;
  uint32_t retry_syn = 0;
  uint32_t retry_error = 0;
  uint32_t second_won = 0;
  uint32_t second_lost = 0;
  uint32_t second_error = 0;
  uint32_t bus_request_blocked = 0;
  uint32_t lock_counter_reset = 0;
  uint32_t session_timeouts = 0;

  void resetMetrics() {
    contention_rate = 0.0;

    // Aggregated Counters
    won_total = 0;
    lost_total = 0;

    // Detailed Counters
    first_syn = 0;
    first_won = 0;
    first_retry = 0;
    first_lost = 0;
    first_error = 0;
    retry_syn = 0;
    retry_error = 0;
    second_won = 0;
    second_lost = 0;
    second_error = 0;
    bus_request_blocked = 0;
    lock_counter_reset = 0;
    session_timeouts = 0;
  }
};

/**
 * Performance and health metrics for the bus layer.
 */
struct BusMetrics {
  // Physical Utilization (%)
  // Utilization is the percentage of time the bus is actively transmitting data
  // versus idle. This is calculated based on the total time spent transmitting
  // data compared to the overall uptime of the bus.
  double utilization = 0.0;

  // Detailed Counters
  uint32_t start_bit_errors = 0;
  uint32_t syn_postponed_count = 0;
  std::chrono::system_clock::time_point last_error_timestamp{};

  // Explicit phase timings
  MetricValues delay;
  MetricValues window;
  MetricValues transmit;
  MetricValues uptime;
  MetricValues syn_postpone;

  void resetMetrics() {
    utilization = 0.0;

    // Detailed Counters
    start_bit_errors = 0;
    syn_postponed_count = 0;
    last_error_timestamp = {};

    // Explicit phase timings
    delay = {};
    window = {};
    transmit = {};
    uptime = {};
    syn_postpone = {};
  }
};

/**
 * Frequency metrics for observed bus participants.
 */
struct DeviceMetrics {
  // Detailed Counters
  uint32_t unknown_devices = 0;
  std::array<uint32_t, 256> masters{};
  std::array<uint32_t, 256> slaves{};

  void resetMetrics() {
    unknown_devices = 0;
    masters.fill(0);
    slaves.fill(0);
  }
};

/**
 * Aggregate system telemetry.
 */
struct SystemMetrics {
  HandlerMetrics handler;
  RequestMetrics request;
  BusMetrics bus;
  DeviceMetrics devices;

  // Quality Score (%)
  // Quality Score is a composite metric that combines error rate and
  // contention rate to provide an overall health indicator of the bus
  // communication. A higher score indicates better performance and
  // reliability.
  double quality = 0.0;
};

}  // namespace metrics

/**
 * Top-level alias for the aggregate metrics.
 */
using Metrics = metrics::SystemMetrics;

/**
 * Global JSON Serialization Helpers
 */

inline std::string toJson(const metrics::HandlerMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"error_rate\":" << m.error_rate
      << ",\"protocol_data_utilization_rate\":"
      << m.protocol_data_utilization_rate

      // Aggregated Counters
      << ",\"messages_total\":" << m.messages_total
      << ",\"error_total\":" << m.error_total
      << ",\"error_passive\":" << m.error_passive
      << ",\"error_reactive\":" << m.error_reactive
      << ",\"error_active\":" << m.error_active
      << ",\"total_data_bytes_sent\":" << m.total_data_bytes_sent
      << ",\"total_protocol_bytes_sent\":"
      << m.total_protocol_bytes_sent

      // Detailed Counters
      << ",\"messages_passive_master_slave\":"
      << m.messages_passive_master_slave
      << ",\"messages_passive_master_master\":"
      << m.messages_passive_master_master
      << ",\"messages_passive_broadcast\":" << m.messages_passive_broadcast
      << ",\"messages_reactive_master_slave\":"
      << m.messages_reactive_master_slave
      << ",\"messages_reactive_master_master\":"
      << m.messages_reactive_master_master
      << ",\"messages_active_master_slave\":" << m.messages_active_master_slave
      << ",\"messages_active_master_master\":"
      << m.messages_active_master_master
      << ",\"messages_active_broadcast\":" << m.messages_active_broadcast
      << ",\"reset_passive_00\":" << m.reset_passive_00
      << ",\"reset_passive_0704\":" << m.reset_passive_0704
      << ",\"reset_passive\":" << m.reset_passive
      << ",\"reset_active_00\":" << m.reset_active_00
      << ",\"reset_active_0704\":" << m.reset_active_0704
      << ",\"reset_active\":" << m.reset_active
      << ",\"error_passive_master\":" << m.error_passive_master
      << ",\"error_passive_master_ack\":" << m.error_passive_master_ack
      << ",\"error_passive_slave\":" << m.error_passive_slave
      << ",\"error_passive_slave_ack\":" << m.error_passive_slave_ack
      << ",\"error_reactive_master\":" << m.error_reactive_master
      << ",\"error_reactive_master_ack\":" << m.error_reactive_master_ack
      << ",\"error_reactive_slave\":" << m.error_reactive_slave
      << ",\"error_reactive_slave_ack\":" << m.error_reactive_slave_ack
      << ",\"error_active_master\":" << m.error_active_master
      << ",\"error_active_master_ack\":" << m.error_active_master_ack
      << ",\"error_active_slave\":" << m.error_active_slave
      << ",\"error_active_slave_ack\":"
      << m.error_active_slave_ack

      // Explicit phase timings
      << ",\"sync\":" << toJson(m.sync) << ",\"write\":" << toJson(m.write)
      << ",\"passive_first\":" << toJson(m.passive_first)
      << ",\"passive_data\":" << toJson(m.passive_data)
      << ",\"active_first\":" << toJson(m.active_first)
      << ",\"active_data\":" << toJson(m.active_data)
      << ",\"callback_won\":" << toJson(m.callback_won)
      << ",\"callback_lost\":" << toJson(m.callback_lost)
      << ",\"callback_reactive\":" << toJson(m.callback_reactive)
      << ",\"callback_telegram\":" << toJson(m.callback_telegram)
      << ",\"callback_error\":" << toJson(m.callback_error)
      << ",\"state_timings\":[";

  // State-machine specific execution timings
  for (int i = 0; i < 15; ++i) {
    if (i > 0) oss << ",";
    oss << toJson(m.state_timings[i]);
  }
  oss << "]}";
  return oss.str();
}

inline std::string toJson(const metrics::RequestMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"contention_rate\":"
      << m.contention_rate

      // Aggregated Counters
      << ",\"won_total\":" << m.won_total << ",\"lost_total\":"
      << m.lost_total

      // Detailed Counters
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

inline std::string toJson(const metrics::BusMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"utilization\":"
      << m.utilization

      // Detailed Counters
      << ",\"start_bit_errors\":" << m.start_bit_errors
      << ",\"syn_postponed_count\":" << m.syn_postponed_count
      << m.syn_postponed_count;

  if (m.last_error_timestamp != std::chrono::system_clock::time_point{}) {
    auto t = std::chrono::system_clock::to_time_t(m.last_error_timestamp);
    oss << ",\"last_error_timestamp\":\""
        << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\"";
  }

  // Explicit phase timings
  oss << ",\"delay\":" << toJson(m.delay) << ",\"window\":" << toJson(m.window)
      << ",\"transmit\":" << toJson(m.transmit)
      << ",\"uptime\":" << toJson(m.uptime)
      << ",\"syn_postpone\":" << toJson(m.syn_postpone) << "}";
  return oss.str();
}

inline std::string toJson(const metrics::DeviceMetrics& m) {
  std::ostringstream oss;
  oss << "{\"unknown_devices\":" << m.unknown_devices << ",\"masters\":{";
  bool first = true;
  for (size_t i = 0; i < 256; ++i) {
    if (m.masters[i] > 0) {
      if (!first) oss << ",";
      // Use hex keys for eBUS addresses
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

/**
 * Serializes the entire SystemMetrics tree to a JSON object string.
 */
inline std::string toJson(const metrics::SystemMetrics& sm) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"handler\":" << toJson(sm.handler)
      << ",\"request\":" << toJson(sm.request) << ",\"bus\":" << toJson(sm.bus)
      << ",\"devices\":" << toJson(sm.devices) << ",\"quality\":" << sm.quality
      << "}";
  return oss.str();
}

}  // namespace ebus