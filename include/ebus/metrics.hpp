/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <cstddef>
#include <string>
#include <vector>

#include "ebus/detail/protocol_limits.hpp"
#include "ebus/types.hpp"

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

namespace ebus {

/**
 * Results of a rolling metric calculation.
 */
struct MetricValues {
  MetricValues() = default;
  MetricValues(uint32_t last, uint32_t max, uint64_t sum, uint64_t cnt)
      : last_us(last), max_us(max), sum_us(sum), count(cnt) {}
  uint32_t last_us = 0;
  uint32_t max_us = 0;
  uint64_t sum_us = 0;
  uint64_t count = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Frequency tracking for error-producing addresses.
 */
struct ErrorAddressStats {
  ErrorAddressStats() = default;
  ErrorAddressStats(uint8_t addr, uint32_t c, uint64_t last_us)
      : address(addr), count(c), last_seen_us(last_us) {}

  uint8_t address = 0xff;
  uint32_t count = 0;
  uint64_t last_seen_us = 0;
};

namespace metrics {

/**
 * Performance and health metrics for the protocol engine.
 */
struct HandlerMetrics {
  // Aggregated Counters
  uint32_t messages_passive = 0;
  uint32_t messages_reactive = 0;
  uint32_t messages_active = 0;
  uint32_t error_passive = 0;
  uint32_t error_reactive = 0;
  uint32_t error_active = 0;
  uint32_t resets_passive = 0;
  uint32_t resets_active = 0;
  uint32_t total_retries = 0;
  uint64_t total_sent_data_bytes = 0;
  uint64_t total_sent_protocol_bytes = 0;
  uint64_t total_observed_data_bytes = 0;
  uint64_t total_observed_protocol_bytes = 0;
  uint32_t invalid_bytes = 0;
  uint8_t last_error_address = 0xff;
  uint8_t last_success_address = 0xff;
  uint64_t last_passive_reset_us = 0;
  uint64_t last_active_reset_us = 0;
  std::array<ErrorAddressStats,
             detail::SystemMetricsLimits::top_error_addresses_count>
      top_errors{};

  // Explicit phase timings
  MetricValues sync;
  MetricValues write;
  MetricValues passive_first;
  MetricValues passive_data;
  MetricValues active_first;
  MetricValues active_data;

  void reset();

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Performance and health metrics for bus requests.
 */
struct RequestMetrics {
  // Aggregated Counters
  uint32_t won_total = 0;
  uint32_t lost_total = 0;
  uint32_t collisions = 0;          // Recoverable eBUS collisions (first_retry)
  uint32_t arbitration_errors = 0;  // Sum of logical/framing errors during arb
  uint32_t first_syn = 0;           // Unexpected SYN (timing issues)

  // Detailed Counters
  uint32_t bus_request_blocked = 0;
  uint32_t lock_counter_reset = 0;
  uint32_t session_timeouts = 0;

  void reset();

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Performance and health metrics for the bus layer.
 */
struct BusMetrics {
  // Detailed Counters
  std::atomic<uint32_t> start_bit_errors = 0;
  std::atomic<uint32_t> syn_postponed_count = 0;
  float utilization = 0.0f;
  bool congestion = false;
  bool high_jitter = false;
  uint64_t last_error_us = 0;  // us since start
  uint64_t uptime_us = 0;      // us since start

  // Explicit phase timings
  MetricValues delay;
  MetricValues window;
  MetricValues transmit;
  MetricValues syn_postpone;

  void reset();

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Frequency metrics for observed bus participants.
 */
struct DeviceMetrics {
  // Detailed Counters
  uint32_t unknown_devices = 0;
  uint32_t identified_devices = 0;

  void reset();

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Performance metrics for the orchestration/application layer.
 */
struct ControllerMetrics {
  uint32_t event_queue_dropped = 0;
  uint32_t max_reactor_queue_size = 0;
  uint32_t max_loop_cycle_us = 0;

  void reset();

  void toJson(detail::JsonWriter& writer) const;
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

  void toJson(detail::JsonWriter& writer) const;
};

}  // namespace metrics

/**
 * Top-level alias for the aggregate metrics.
 */
using Metrics = metrics::SystemMetrics;

}  // namespace ebus