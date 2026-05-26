/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

#include <chrono>
#include <ebus/status.hpp>

#include "utils/json_utils.hpp"

namespace ebus::detail {

namespace {
uint64_t getNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
}  // namespace

BusMonitor::BusMonitor() = default;

void BusMonitor::resetMetrics() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_acc_.reset();
  request_acc_.reset();
  bus_acc_.reset();
  controller_acc_.reset();
  device_acc_.reset();

  sync.reset();
  write.reset();
  passive_first.reset();
  passive_data.reset();
  active_first.reset();
  active_data.reset();
  syn_postpone.reset();

  delay.reset();
  window.reset();
  transmit.reset();
  uptime_start_ = Clock::now();
  total_low_bits_ = 0;
  last_history_low_bits_ = 0;
  last_history_uptime_us_ = 0;

  handler_history_.clear();
  request_history_.clear();
}

ebus::metrics::SystemMetrics BusMonitor::getMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  metrics::SystemMetrics sm;

  // 1. Populate Handler Part
  metrics::HandlerMetrics& hm = sm.handler;
  hm = handler_acc_;

  // Map Timing
  hm.sync = sync.getValues();
  hm.write = write.getValues();
  hm.passive_first = passive_first.getValues();
  hm.passive_data = passive_data.getValues();
  hm.active_first = active_first.getValues();
  hm.active_data = active_data.getValues();

  // 2. Populate Request Part
  metrics::RequestMetrics& rm = sm.request;
  rm = request_acc_;

  // 3. Populate Bus Part
  metrics::BusMetrics& bm = sm.bus;
  bm = bus_acc_;

  // Map Timing
  bm.delay = delay.getValues();
  bm.window = window.getValues();
  bm.transmit = transmit.getValues();
  bm.syn_postpone = syn_postpone.getValues();

  auto now = Clock::now();
  uint64_t uptime_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - uptime_start_)
          .count();
  bm.uptime_us = uptime_us;

  // Physical Utilization Logic (Congestion detection)
  if (uptime_us > 0) {
    uint64_t total_low_us =
        (total_low_bits_ * Physical::bit_time_num) / Physical::bit_time_den;
    float utilization =
        (static_cast<float>(total_low_us) / static_cast<float>(uptime_us)) *
        100.0f;
    // Congestion Logic: > 70% for > 10 seconds
    // If a single uptime sample already indicates the bus has been up for
    // at least 10s (samples are in microseconds), treat high utilization
    // as sustained congestion immediately. Otherwise fall back to the
    // time-point based detection across successive calls.
    constexpr uint64_t kTenSecondsUs = 10000000;
    if (utilization > 70.0f) {
      if (uptime_us >= kTenSecondsUs) {
        congestion_active_ = true;
      } else {
        if (congestion_start_point_ == Clock::time_point{}) {
          congestion_start_point_ = now;
        } else {
          auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                              now - congestion_start_point_)
                              .count();
          if (duration >= 10) {
            congestion_active_ = true;
          }
        }
      }
    } else {
      congestion_start_point_ = {};
      congestion_active_ = false;
    }
  }
  bm.congestion = congestion_active_;

  // High Jitter Logic: If a SYN took > 10ms longer than expected
  bm.high_jitter = bm.syn_postpone.max_us > 10000;

  // 4. Populate Device Part
  metrics::DeviceMetrics& dm = sm.devices;
  dm = device_acc_;

  // 5. Populate Controller Part
  sm.controller = controller_acc_;

  return sm;
}

float BusMonitor::getBusUtilization() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto now = Clock::now();
  auto total_uptime_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - uptime_start_)
          .count();

  if (total_uptime_us > 0) {
    uint64_t total_low_us =
        (total_low_bits_ * Physical::bit_time_num) / Physical::bit_time_den;
    return (static_cast<float>(total_low_us) /
            static_cast<float>(total_uptime_us)) *
           100.0f;
  }
  return 0.0f;
}

void BusMonitor::updateUtilizationHistory() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto now = Clock::now();
  auto total_uptime_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - uptime_start_)
          .count();

  uint64_t delta_bits = total_low_bits_ - last_history_low_bits_;
  uint64_t delta_time = total_uptime_us - last_history_uptime_us_;

  uint64_t delta_low_us =
      (delta_bits * Physical::bit_time_num) / Physical::bit_time_den;

  float current_util = (delta_time > 0) ? (static_cast<float>(delta_low_us) /
                                           static_cast<float>(delta_time)) *
                                              100.0f
                                        : 0.0f;

  utilization_history_.push_back(static_cast<float>(current_util));

  last_history_low_bits_ = total_low_bits_;
  last_history_uptime_us_ = total_uptime_us;
}

std::vector<float> BusMonitor::getUtilizationHistory() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  std::vector<float> history;
  utilization_history_.forEach([&](float val) { history.push_back(val); });
  return history;
}

void BusMonitor::logHandlerTransition(HandlerState from, HandlerState to) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_history_.push_back({from, to, getNowMs()});
}

void BusMonitor::logRequestTransition(RequestState from, RequestState to) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  request_history_.push_back({from, to, getNowMs()});
}

void BusMonitor::recordBusError() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto now = Clock::now();
  bus_acc_.last_error_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - uptime_start_)
          .count();
}

void BusMonitor::recordLowBits(uint32_t bits) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  total_low_bits_ += bits;
}

void BusMonitor::clearHistory() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_history_.clear();
  request_history_.clear();
  utilization_history_.clear();

  auto now = Clock::now();
  auto uptime_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - uptime_start_)
          .count();
  last_history_low_bits_ = total_low_bits_;
  last_history_uptime_us_ = uptime_us;
}

}  // namespace ebus::detail

namespace ebus {

// --- Metrics Implementations ---

void MetricValues::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "last_us", static_cast<uint64_t>(last_us), first_field);
  append_field(json, "max_us", static_cast<uint64_t>(max_us), first_field);
  append_field(json, "count", static_cast<uint64_t>(count), first_field);
  json += "}";
}

void metrics::HandlerMetrics::toJson(std::string& json) const {
  uint32_t m_total = messages_passive + messages_active + messages_reactive;
  uint32_t e_total = error_passive + error_reactive + error_active;

  json += "{";
  bool first_field = true;
  float e_rate =
      (m_total > 0 || e_total > 0)
          ? (static_cast<float>(e_total) / (m_total + e_total)) * 100.0f
          : 0.0f;
  append_json_float_custom_precision(json, "error_rate", e_rate, 2,
                                     first_field);
  float pd_util = (total_protocol_bytes_sent > 0)
                      ? (static_cast<float>(total_data_bytes_sent) /
                         total_protocol_bytes_sent) *
                            100.0f
                      : 0.0f;
  append_json_float_custom_precision(json, "protocol_data_utilization_rate",
                                     pd_util, 2, first_field);

  float global_eff = (total_observed_protocol_bytes > 0)
                         ? (static_cast<float>(total_observed_data_bytes) /
                            total_observed_protocol_bytes) *
                               100.0f
                         : 0.0f;
  append_json_float_custom_precision(json, "global_payload_efficiency",
                                     global_eff, 2, first_field);

  append_field(json, "messages_total",
               static_cast<uint64_t>(messages_passive + messages_active +
                                     messages_reactive),
               first_field);
  append_field(json, "messages_passive",
               static_cast<uint64_t>(messages_passive), first_field);
  append_field(json, "messages_reactive",
               static_cast<uint64_t>(messages_reactive), first_field);
  append_field(json, "messages_active", static_cast<uint64_t>(messages_active),
               first_field);
  append_field(
      json, "error_total",
      static_cast<uint64_t>(error_passive + error_reactive + error_active),
      first_field);
  append_field(json, "error_passive", static_cast<uint64_t>(error_passive),
               first_field);
  append_field(json, "error_reactive", static_cast<uint64_t>(error_reactive),
               first_field);
  append_field(json, "error_active", static_cast<uint64_t>(error_active),
               first_field);
  append_field(json, "resets_passive", static_cast<uint64_t>(resets_passive),
               first_field);
  append_field(json, "resets_active", static_cast<uint64_t>(resets_active),
               first_field);
  append_field(json, "total_observed_data_bytes", total_observed_data_bytes,
               first_field);
  append_field(json, "total_observed_protocol_bytes",
               total_observed_protocol_bytes, first_field);

  append_field(json, "total_data_bytes_sent",
               static_cast<uint64_t>(total_data_bytes_sent), first_field);
  append_field(json, "total_protocol_bytes_sent",
               static_cast<uint64_t>(total_protocol_bytes_sent), first_field);

  append_key(json, "sync", first_field);
  sync.toJson(json);
  append_key(json, "write", first_field);
  write.toJson(json);
  append_key(json, "passive_first", first_field);
  passive_first.toJson(json);
  append_key(json, "passive_data", first_field);
  passive_data.toJson(json);
  append_key(json, "active_first", first_field);
  active_first.toJson(json);
  append_key(json, "active_data", first_field);
  active_data.toJson(json);

  json += "}";
}

void metrics::RequestMetrics::toJson(std::string& json) const {
  uint32_t attempts =
      won_total + lost_total + collisions + arbitration_errors + first_syn;
  json += "{";
  bool first_field = true;
  float cont_rate =
      (attempts > 0)
          ? (static_cast<float>(lost_total + collisions) / attempts) * 100.0f
          : 0.0f;
  float coll_rate = (attempts > 0)
                        ? (static_cast<float>(collisions) / attempts) * 100.0f
                        : 0.0f;

  append_json_float_custom_precision(json, "contention_rate", cont_rate, 2,
                                     first_field);
  append_json_float_custom_precision(json, "collision_rate", coll_rate, 2,
                                     first_field);
  append_field(json, "won_total", static_cast<uint64_t>(won_total),
               first_field);
  append_field(json, "lost_total", static_cast<uint64_t>(lost_total),
               first_field);
  append_field(json, "collisions", static_cast<uint64_t>(collisions),
               first_field);
  append_field(json, "arbitration_errors",
               static_cast<uint64_t>(arbitration_errors), first_field);
  append_field(json, "first_syn", static_cast<uint64_t>(first_syn),
               first_field);
  json += "}";
}

void metrics::BusMetrics::toJson(std::string& json) const {
  // Note: we'd need total_low_bits_ here for perfect accuracy, but uptime_us
  // and the other timing samples provide sufficient operational context.
  // BusMonitor::getBusUtilization() provides the real-time calculated value.

  json += "{";
  bool first_field = true;
  // We keep the utilization key for dashboards, but app will calculate from
  // raw.
  append_json_value_raw(json, "utilization", "null", first_field);
  append_field(json, "start_bit_errors",
               static_cast<uint64_t>(start_bit_errors), first_field);
  append_field(json, "syn_postponed_count",
               static_cast<uint64_t>(syn_postponed_count), first_field);
  append_field(json, "congestion", congestion, first_field);
  append_field(json, "high_jitter", high_jitter, first_field);
  append_field(json, "last_error_us", static_cast<uint64_t>(last_error_us),
               first_field);
  append_field(json, "uptime_us", static_cast<uint64_t>(uptime_us),
               first_field);

  append_key(json, "delay", first_field);
  delay.toJson(json);
  append_key(json, "window", first_field);
  window.toJson(json);
  append_key(json, "transmit", first_field);
  transmit.toJson(json);
  append_key(json, "syn_postpone", first_field);
  syn_postpone.toJson(json);
  json += "}";
}

void metrics::DeviceMetrics::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "unknown_devices", static_cast<uint64_t>(unknown_devices),
               first_field);

  json += "}";
}

void metrics::ControllerMetrics::toJson(std::string& json) const {
  json += "{\"public_queue_dropped\":";
  char buffer[32];
  auto [ptr, ec] =
      std::to_chars(buffer, buffer + sizeof(buffer), public_queue_dropped);
  if (ec == std::errc{}) {
    json.append(buffer, ptr - buffer);
  } else {
    json += "null";
  }
  json += "}";
}

void metrics::SystemMetrics::toJson(std::string& json) const {
  // Pre-calculate rates for the composite Quality score
  uint32_t m_total = handler.messages_passive + handler.messages_active +
                     handler.messages_reactive;
  uint32_t e_total =
      handler.error_passive + handler.error_reactive + handler.error_active;
  float e_rate =
      (m_total > 0 || e_total > 0)
          ? (static_cast<float>(e_total) / (m_total + e_total)) * 100.0f
          : 0.0f;

  uint32_t attempts = request.won_total + request.lost_total +
                      request.collisions + request.arbitration_errors +
                      request.first_syn;
  float cont_rate =
      (attempts > 0)
          ? (static_cast<float>(request.lost_total + request.collisions) /
             attempts) *
                100.0f
          : 0.0f;

  float quality = (100.0f - e_rate) * (1.0f - (cont_rate / 100.0f));

  json += "{";
  bool first_field = true;
  append_key(json, "handler", first_field);
  handler.toJson(json);
  append_key(json, "request", first_field);
  request.toJson(json);
  append_key(json, "bus", first_field);
  bus.toJson(json);
  append_key(json, "devices", first_field);
  devices.toJson(json);
  append_key(json, "controller", first_field);
  controller.toJson(json);
  append_json_float_custom_precision(json, "quality", quality, 2, first_field);
  json += "}";
}

// --- Status Implementations ---

namespace {
void threadToJson(const ThreadStatus& s, std::string& json) {
  json += "{";
  bool first_field = true;
  append_field(json, "name",
               std::string_view(s.name.empty() ? "unknown" : s.name),
               first_field);
  append_field(json, "stack_size", static_cast<int64_t>(s.task_stack_bytes),
               first_field);
  append_field(json, "stack_free",
               static_cast<int64_t>(s.task_stack_free_bytes), first_field);
  json += "}";
}

void queueToJson(const ThreadStatus& thread, size_t size, size_t capacity,
                 std::string& json) {
  json += "{\"thread\":";
  thread.toJson(json);
  json += ",";
  bool first_field = true;  // Reset for inner fields
  append_field(json, "queue_size", static_cast<uint64_t>(size), first_field);
  append_field(json, "queue_capacity", static_cast<uint64_t>(capacity),
               first_field);
  json += "}";
}
}  // namespace

void ThreadStatus::toJson(std::string& json) const {
  threadToJson(*this, json);
}

void ControllerStatus::toJson(std::string& json) const {
  json += "{\"thread\":";
  thread.toJson(json);
  json += "}";
}

void BusStatus::toJson(std::string& json) const {
  json += "{\"bus_thread\":";
  bus_thread.toJson(json);
  json += ",\"syn_thread\":";
  syn_thread.toJson(json);
  json += "}";
}

void BusHandlerStatus::toJson(std::string& json) const {
  queueToJson(thread, queue_size, queue_capacity, json);
}

void SchedulerStatus::toJson(std::string& json) const {
  queueToJson(thread, queue_size, queue_capacity, json);
}

void ClientManagerStatus::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_key(json, "thread", first_field);
  thread.toJson(json);
  append_field(json, "queue_size", static_cast<uint64_t>(queue_size),
               first_field);
  append_field(json, "queue_capacity", static_cast<uint64_t>(queue_capacity),
               first_field);
  append_field(json, "session_active", session_active, first_field);
  append_field(json, "session_state", session_state, first_field);
  append_field(json, "last_error", last_error, first_field);
  json += "}";
}

void DeviceManagerStatus::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "identified_count",
               static_cast<uint64_t>(identified_count), first_field);
  append_field(json, "unknown_count", static_cast<uint64_t>(unknown_count),
               first_field);
  json += "}";
}

void DeviceScannerStatus::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "is_scanning", is_scanning, first_field);
  append_field(json, "full_scan_active", full_scan_active, first_field);
  append_field(json, "full_scan_address",
               static_cast<uint64_t>(full_scan_address), first_field);
  append_field(json, "scan_on_startup_enabled", scan_on_startup_enabled,
               first_field);
  append_field(json, "startup_scan_count",
               static_cast<int64_t>(startup_scan_count), first_field);
  append_field(json, "manual_queue_size",
               static_cast<uint64_t>(manual_queue_size), first_field);
  append_field(json, "startup_queue_size",
               static_cast<uint64_t>(startup_queue_size), first_field);
  json += "}";
}

void PollManagerStatus::toJson(std::string& json) const {
  json += "{\"item_count\":";
  char buffer[32];
  auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), item_count);
  if (ec == std::errc{}) {
    json.append(buffer, ptr - buffer);
  } else {
    json += "null";
  }
  json += "}";
}

void SystemResources::QueueInfo::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "name", name, first_field);
  append_field(json, "size", static_cast<uint64_t>(size), first_field);
  append_field(json, "capacity", static_cast<uint64_t>(capacity), first_field);
  json += "}";
}

void SystemResources::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "timestamp_ms", static_cast<uint64_t>(timestamp_ms),
               first_field);
  json += ",\"threads\":[";
  for (size_t i = 0; i < threads.size(); ++i) {
    if (i > 0) json += ",";
    threads[i].toJson(json);
  }
  json += "],\"queues\":[";
  for (size_t i = 0; i < queues.size(); ++i) {
    if (i > 0) json += ",";
    queues[i].toJson(json);
  }
  json += "]}";
}

void ServiceStatus::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "last_update_timestamp_ms",
               static_cast<uint64_t>(last_update_timestamp_ms), first_field);
  append_key(json, "controller", first_field);
  controller.toJson(json);
  append_key(json, "bus", first_field);
  bus.toJson(json);
  append_key(json, "bus_handler", first_field);
  bus_handler.toJson(json);
  append_key(json, "scheduler", first_field);
  scheduler.toJson(json);
  append_key(json, "client_manager", first_field);
  client_manager.toJson(json);
  append_key(json, "device_manager", first_field);
  device_manager.toJson(json);
  append_key(json, "device_scanner", first_field);
  device_scanner.toJson(json);
  append_key(json, "poll_manager", first_field);
  poll_manager.toJson(json);
  json += "}";
}

/**
 * To implement the zero-allocation requirement, we would need to pass
 * the actual BusMonitor to this function, as ServiceStatus no longer
 * holds the history vectors.
 */
std::string serializeServiceStatus(const ServiceStatus& status,
                                   detail::BusMonitor* monitor,
                                   bool reset_histories) {
  std::string json;
  json.reserve(monitor ? 8192 : 2048);
  serializeServiceStatus(json, status, monitor, reset_histories);
  return json;
}

void serializeServiceStatus(std::string& json_str, const ServiceStatus& status,
                            detail::BusMonitor* monitor, bool reset_histories) {
  status.toJson(json_str);

  if (monitor) {
    // Remove trailing '}' to append history
    if (!json_str.empty() && json_str.back() == '}') json_str.pop_back();

    json_str += ",\"handler_history\":[";
    bool first = true;
    monitor->handler_history_.forEach([&](const HandlerTransition& t) {
      if (!first) json_str += ",";
      t.toJson(json_str);
      first = false;
    });
    json_str += "],\"request_history\":[";
    first = true;
    monitor->request_history_.forEach([&](const RequestTransition& t) {
      if (!first) json_str += ",";
      t.toJson(json_str);
      first = false;
    });
    json_str += "]";
    json_str += "}";  // Re-add trailing '}'

    if (reset_histories) {
      monitor->clearHistory();
    }
  }
}

}  // namespace ebus