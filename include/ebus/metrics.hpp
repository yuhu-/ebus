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
  double error_rate = 0.0;

  // Aggregated Counters
  uint32_t messages_total = 0;
  uint32_t error_total = 0;
  uint32_t error_passive = 0;
  uint32_t error_reactive = 0;
  uint32_t error_active = 0;

  // Individual Detailed Counters
  struct {
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
  } counters;

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
};

/**
 * Performance and health metrics for bus requests.
 */
struct RequestMetrics {
  // Contention Rate (%)
  double contention_rate = 0.0;

  // Aggregated Counters
  uint32_t won_total = 0;
  uint32_t lost_total = 0;

  // Individual Detailed Counters
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
};

/**
 * Performance and health metrics for the bus layer.
 */
struct BusMetrics {
  // Physical Utilization (%)
  double utilization = 0.0;

  // Detailed Counters
  uint32_t start_bit_errors = 0;

  // Explicit phase timings
  MetricValues delay;
  MetricValues window;
  MetricValues transmit;
  MetricValues uptime;
};

/**
 * Aggregate system telemetry.
 */
struct SystemMetrics {
  HandlerMetrics handler;
  RequestMetrics request;
  BusMetrics bus;

  // Quality Score (%)
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
      << ",\"messages_total\":" << m.messages_total
      << ",\"error_total\":" << m.error_total
      << ",\"error_passive\":" << m.error_passive
      << ",\"error_active\":" << m.error_active
      << ",\"error_reactive\":" << m.error_reactive
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

inline std::string toJson(const metrics::BusMetrics& m) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"utilization\":" << m.utilization
      << ",\"start_bit_errors\":" << m.start_bit_errors
      << ",\"delay\":" << toJson(m.delay) << ",\"window\":" << toJson(m.window)
      << ",\"transmit\":" << toJson(m.transmit)
      << ",\"uptime\":" << toJson(m.uptime) << "}";
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
      << ",\"quality\":" << sm.quality << "}";
  return oss.str();
}

/**
 * Math engine for online statistics calculation using Welford's algorithm.
 * This class is platform-independent and does not depend on chrono.
 */
class RollingStats {
 public:
  RollingStats();
  virtual ~RollingStats() = default;

  /**
   * Adds a new data point to the dataset and updates rolling statistics.
   */
  void addSample(double value);

  /**
   * Resets all accumulated data.
   */
  void reset();

  MetricValues getValues() const;

  /**
   * Returns the sum of all samples added.
   */
  double getSum() const { return mean_ * count_; }

  double getLast() const { return last_; }
  double getMean() const { return mean_; }
  uint64_t getCount() const { return count_; }
  double getStdDev() const;

 protected:
  double last_;
  double min_;
  double max_;
  uint64_t count_;
  double mean_;
  double m2_;  // Internal state for variance calculation
};

}  // namespace ebus