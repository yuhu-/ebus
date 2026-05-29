/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

#include <chrono>
#include <ebus/detail/json_writer.hpp>
#include <ebus/status.hpp>
#include <ebus/utils.hpp>

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

void BusMonitor::fetchMetrics(
    const std::function<void(const Metrics&)>& callback) const {
  metrics::SystemMetrics sm;
  {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    // Populate snapshot while holding the lock

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
    uint64_t uptime_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             now - uptime_start_)
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
  }

  // Execute callback outside the lock to protect the Hot Path
  if (callback) {
    callback(sm);
  }
}

void BusMonitor::fetchHistory(
    const std::function<void(const HandlerHistory&, const RequestHistory&,
                             const UtilizationHistory&)>& callback) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (callback) {
    callback(handler_history_, request_history_, utilization_history_);
  }
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

void BusMonitor::fetchUtilizationHistory(
    const std::function<void(float)>& callback) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (callback) {
    utilization_history_.forEach(callback);
  }
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

void MetricValues::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("last_us", static_cast<uint64_t>(last_us));
  writer.writeField("max_us", static_cast<uint64_t>(max_us));
  writer.writeField("count", static_cast<uint64_t>(count));
  writer.endObject();
}

void metrics::HandlerMetrics::toJson(const JsonChunkVisitor& visitor) const {
  uint32_t m_total = messages_passive + messages_active + messages_reactive;
  uint32_t e_total = error_passive + error_reactive + error_active;

  float e_rate =
      (m_total > 0 || e_total > 0)
          ? (static_cast<float>(e_total) / (m_total + e_total)) * 100.0f
          : 0.0f;

  float pd_util = (total_protocol_bytes_sent > 0)
                      ? (static_cast<float>(total_data_bytes_sent) /
                         total_protocol_bytes_sent) *
                            100.0f
                      : 0.0f;

  float global_eff = (total_observed_protocol_bytes > 0)
                         ? (static_cast<float>(total_observed_data_bytes) /
                            total_observed_protocol_bytes) *
                               100.0f
                         : 0.0f;

  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeFieldFloat("error_rate", e_rate);
  writer.writeFieldFloat("protocol_data_utilization_rate", pd_util);
  writer.writeFieldFloat("global_payload_efficiency", global_eff);
  writer.writeField("messages_total", static_cast<uint64_t>(m_total));
  writer.writeField("messages_passive",
                    static_cast<uint64_t>(messages_passive));
  writer.writeField("messages_reactive",
                    static_cast<uint64_t>(messages_reactive));
  writer.writeField("messages_active", static_cast<uint64_t>(messages_active));
  writer.writeField("error_total", static_cast<uint64_t>(e_total));
  writer.writeField("error_passive", static_cast<uint64_t>(error_passive));
  writer.writeField("error_reactive", static_cast<uint64_t>(error_reactive));
  writer.writeField("error_active", static_cast<uint64_t>(error_active));
  writer.writeField("resets_passive", static_cast<uint64_t>(resets_passive));
  writer.writeField("resets_active", static_cast<uint64_t>(resets_active));
  writer.writeField("total_observed_data_bytes", total_observed_data_bytes);
  writer.writeField("total_observed_protocol_bytes",
                    total_observed_protocol_bytes);
  writer.writeField("total_data_bytes_sent",
                    static_cast<uint64_t>(total_data_bytes_sent));
  writer.writeField("total_protocol_bytes_sent",
                    static_cast<uint64_t>(total_protocol_bytes_sent));

  writer.appendKey("sync");
  sync.toJson(visitor);
  writer.appendKey("write");
  write.toJson(visitor);
  writer.appendKey("passive_first");
  passive_first.toJson(visitor);
  writer.appendKey("passive_data");
  passive_data.toJson(visitor);
  writer.appendKey("active_first");
  active_first.toJson(visitor);
  writer.appendKey("active_data");
  active_data.toJson(visitor);
  writer.endObject();
}

void metrics::RequestMetrics::toJson(const JsonChunkVisitor& visitor) const {
  uint32_t attempts =
      won_total + lost_total + collisions + arbitration_errors + first_syn;
  float cont_rate =
      (attempts > 0)
          ? (static_cast<float>(lost_total + collisions) / attempts) * 100.0f
          : 0.0f;
  float coll_rate = (attempts > 0)
                        ? (static_cast<float>(collisions) / attempts) * 100.0f
                        : 0.0f;
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeFieldFloat("contention_rate", cont_rate);
  writer.writeFieldFloat("collision_rate", coll_rate);
  writer.writeField("won_total", static_cast<uint64_t>(won_total));
  writer.writeField("lost_total", static_cast<uint64_t>(lost_total));
  writer.writeField("collisions", static_cast<uint64_t>(collisions));
  writer.writeField("arbitration_errors",
                    static_cast<uint64_t>(arbitration_errors));
  writer.writeField("first_syn", static_cast<uint64_t>(first_syn));
  writer.endObject();
}

void metrics::BusMetrics::toJson(const JsonChunkVisitor& visitor) const {
  // Note: we'd need total_low_bits_ here for perfect accuracy, but uptime_us
  // and the other timing samples provide sufficient operational context.
  // BusMonitor::getBusUtilization() provides the real-time calculated value.
  detail::JsonWriter writer(visitor);
  writer.startObject();
  // We keep the utilization key for dashboards, but app will calculate from
  // raw.
  writer.writeField("utilization", "null");  // Raw value, not a number
  writer.writeField("start_bit_errors",
                    static_cast<uint64_t>(start_bit_errors));
  writer.writeField("syn_postponed_count",
                    static_cast<uint64_t>(syn_postponed_count));
  writer.writeField("congestion", congestion);
  writer.writeField("high_jitter", high_jitter);
  writer.writeField("last_error_us", static_cast<uint64_t>(last_error_us));
  writer.writeField("uptime_us", static_cast<uint64_t>(uptime_us));

  writer.appendKey("delay");
  delay.toJson(visitor);
  writer.appendKey("window");
  window.toJson(visitor);
  writer.appendKey("transmit");
  transmit.toJson(visitor);
  writer.appendKey("syn_postpone");
  syn_postpone.toJson(visitor);
  writer.endObject();
}

void metrics::DeviceMetrics::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("unknown_devices", static_cast<uint64_t>(unknown_devices));
  writer.endObject();
}

void metrics::ControllerMetrics::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("event_queue_dropped",
                    static_cast<uint64_t>(event_queue_dropped));
  writer.endObject();
}

void metrics::SystemMetrics::toJson(const JsonChunkVisitor& visitor) const {
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

  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("handler");
  handler.toJson(visitor);
  writer.appendKey("request");
  request.toJson(visitor);
  writer.appendKey("bus");
  bus.toJson(visitor);
  writer.appendKey("devices");
  devices.toJson(visitor);
  writer.appendKey("controller");
  controller.toJson(visitor);
  writer.writeFieldFloat("quality", quality);
  writer.endObject();
}

// --- Status Implementations ---

void ThreadStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("name", name.empty() ? "unknown" : name);
  writer.writeField("stack_size", static_cast<int64_t>(task_stack_bytes));
  writer.writeField("stack_free", static_cast<int64_t>(task_stack_free_bytes));
  writer.endObject();
}

void ControllerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("thread");
  thread.toJson(visitor);
  writer.endObject();
}

void BusStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("bus_thread");
  bus_thread.toJson(visitor);
  writer.appendKey("syn_thread");
  syn_thread.toJson(visitor);
  writer.endObject();
}

void BusHandlerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("thread");
  thread.toJson(visitor);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.endObject();
}

void SchedulerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("thread");
  thread.toJson(visitor);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.endObject();
}

void ClientManagerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.appendKey("thread");
  thread.toJson(visitor);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.writeField("session_active", session_active);
  writer.writeField("session_state", session_state);
  writer.writeField("last_error", last_error);
  writer.endObject();
}

void DeviceManagerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("identified_count",
                    static_cast<uint64_t>(identified_count));
  writer.writeField("unknown_count", static_cast<uint64_t>(unknown_count));
  writer.endObject();
}

void DeviceScannerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("is_scanning", is_scanning);
  writer.writeField("full_scan_active", full_scan_active);
  writer.writeField("full_scan_address",
                    static_cast<uint64_t>(full_scan_address));
  writer.writeField("scan_on_startup_enabled", scan_on_startup_enabled);
  writer.writeField("startup_scan_count",
                    static_cast<uint64_t>(startup_scan_count));
  writer.writeField("manual_queue_size",
                    static_cast<uint64_t>(manual_queue_size));
  writer.writeField("startup_queue_size",
                    static_cast<uint64_t>(startup_queue_size));
  writer.endObject();
}

void PollManagerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("item_count", static_cast<uint64_t>(item_count));
  writer.endObject();
}

void SystemResources::QueueInfo::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("name", name);
  writer.writeField("size", static_cast<uint64_t>(size));
  writer.writeField("capacity", static_cast<uint64_t>(capacity));
  writer.endObject();
}

void SystemResources::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("timestamp_ms", timestamp_ms);
  writer.appendKey("threads");
  writer.startArray();
  for (const auto& t : threads) t.toJson(visitor);
  writer.endArray();
  writer.appendKey("queues");
  writer.startArray();
  for (const auto& q : queues) q.toJson(visitor);
  writer.endArray();
  writer.endObject();
}

void ServiceStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("last_update_timestamp_ms", last_update_timestamp_ms);
  writer.appendKey("controller");
  controller.toJson(visitor);
  writer.appendKey("bus");
  bus.toJson(visitor);
  writer.appendKey("bus_handler");
  bus_handler.toJson(visitor);
  writer.appendKey("scheduler");
  scheduler.toJson(visitor);
  writer.appendKey("client_manager");
  client_manager.toJson(visitor);
  writer.appendKey("device_manager");
  device_manager.toJson(visitor);
  writer.appendKey("device_scanner");
  device_scanner.toJson(visitor);
  writer.appendKey("poll_manager");
  poll_manager.toJson(visitor);
  writer.endObject();
}

void serializeServiceStatus(const JsonChunkVisitor& visitor,
                            const ServiceStatus& status,
                            detail::BusMonitor* monitor, bool reset_histories) {
  if (!visitor) return;

  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("last_update_timestamp_ms",
                    status.last_update_timestamp_ms);
  writer.appendKey("controller");
  status.controller.toJson(visitor);
  writer.appendKey("bus");
  status.bus.toJson(visitor);
  writer.appendKey("bus_handler");
  status.bus_handler.toJson(visitor);
  writer.appendKey("scheduler");
  status.scheduler.toJson(visitor);
  writer.appendKey("client_manager");
  status.client_manager.toJson(visitor);
  writer.appendKey("device_manager");
  status.device_manager.toJson(visitor);
  writer.appendKey("device_scanner");
  status.device_scanner.toJson(visitor);
  writer.appendKey("poll_manager");
  status.poll_manager.toJson(visitor);

  if (monitor) {
    monitor->fetchHistory([&](const auto& h_hist, const auto& r_hist,
                              const auto& u_hist) {
      writer.appendKey("handler_history");
      writer.startArray();
      h_hist.forEach([&](const HandlerTransition& t) { t.toJson(visitor); });
      writer.endArray();

      writer.appendKey("request_history");
      writer.startArray();
      r_hist.forEach([&](const RequestTransition& t) { t.toJson(visitor); });
      writer.endArray();

      writer.appendKey("utilization_history");
      writer.startArray();
      u_hist.forEach([&](float util) { writer.writeValueFloat(util); });
      writer.endArray();
    });

    if (reset_histories) {
      monitor->clearHistory();
    }
  }
  writer.endObject();
}

}  // namespace ebus