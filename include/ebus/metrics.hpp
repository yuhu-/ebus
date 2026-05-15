/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ebus/detail/protocol_limits.hpp"
#include "ebus/types.hpp"

namespace ebus {

/**
 * Results of a rolling metric calculation.
 */
struct MetricValues {
  float last = 0.0f;
  float min = 0.0f;
  float max = 0.0f;
  float mean = 0.0f;
  float stddev = 0.0f;
  uint32_t count = 0;

  /**
   * @brief Serializes MetricValues to a JSON object string.
   */
  std::string toJson() const;
};

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
  float error_rate = 0.0f;

  // Protocol Data Utilization Rate (%)
  // This metric represents the efficiency of data transmission, calculated as
  // the ratio of protocol bytes (address + data) to total bytes sent on the bus
  // (including overhead like SYN, ACK, etc.). A higher utilization rate
  // indicates more efficient use of the bus bandwidth for actual data
  // transmission.
  float protocol_data_utilization_rate = 0.0f;

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
  std::array<MetricValues, detail::FsmLimits::num_handler_states> state_timings;

  void reset() {
    error_rate = 0.0f;
    protocol_data_utilization_rate = 0.0f;

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

  std::string toJson() const;
};

/**
 * Performance and health metrics for bus requests.
 */
struct RequestMetrics {
  // Contention Rate (%)
  // Contention happens when we lose arbitration (lost or retry) on the
  // first attempt. This is calculated as the number of contention events
  // divided by the total number of bus request attempts.
  float contention_rate = 0.0f;

  // Collision Rate (%)
  // Percentage of total transmit attempts that encountered a collision
  // (arbitration lost or retry required).
  float collision_rate = 0.0f;

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

  void reset() {
    contention_rate = 0.0f;
    collision_rate = 0.0f;

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

  std::string toJson() const;
};

/**
 * Performance and health metrics for the bus layer.
 */
struct BusMetrics {
  // Physical Utilization (%)
  // Utilization is the percentage of time the bus is actively transmitting data
  // versus idle. This is calculated based on the total time spent transmitting
  // data compared to the overall uptime of the bus.
  float utilization = 0.0f;

  // Detailed Counters
  uint32_t start_bit_errors = 0;
  uint32_t syn_postponed_count = 0;
  bool congestion = false;
  bool high_jitter = false;
  uint64_t last_error_timestamp = 0;  // ms since epoch

  // Explicit phase timings
  MetricValues delay;
  MetricValues window;
  MetricValues transmit;
  MetricValues uptime;
  MetricValues syn_postpone;

  void reset() {
    utilization = 0.0f;

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

  std::string toJson() const;
};

/**
 * Frequency metrics for observed bus participants.
 */
struct DeviceMetrics {
  // Detailed Counters
  uint32_t unknown_devices = 0;
  std::array<uint32_t, 256> masters{};
  std::array<uint32_t, 256> slaves{};

  void reset() {
    unknown_devices = 0;
    masters.fill(0);
    slaves.fill(0);
  }

  std::string toJson() const;
};

/**
 * Performance metrics for the orchestration/application layer.
 */
struct ControllerMetrics {
  uint32_t public_queue_dropped = 0;

  void reset() { public_queue_dropped = 0; }
  std::string toJson() const;
};

/**
 * Aggregate system telemetry.
 */
struct SystemMetrics {
  HandlerMetrics handler;
  RequestMetrics request;
  BusMetrics bus;
  DeviceMetrics devices;
  ControllerMetrics controller;

  // Quality Score (%)
  // Quality Score is a composite metric that combines error rate and
  // contention rate to provide an overall health indicator of the bus
  // communication. A higher score indicates better performance and
  // reliability.
  float quality = 0.0f;

  std::string toJson() const;
};

}  // namespace metrics

/**
 * Top-level alias for the aggregate metrics.
 */
using Metrics = metrics::SystemMetrics;

}  // namespace ebus