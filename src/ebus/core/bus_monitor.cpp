/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_monitor.hpp"

#include <algorithm>
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
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  last_history_low_bits_ = 0;
  last_history_uptime_us_ = 0;

  handler_history_.clear();
  request_history_.clear();
#endif
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

#ifndef EBUS_MINIMAL_DIAGNOSTICS
void BusMonitor::fetchHistory(
    const std::function<void(const HandlerHistory&, const RequestHistory&,
                             const UtilizationHistory&)>& callback) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (callback) {
    callback(handler_history_, request_history_, utilization_history_);
  }
}
#endif

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
#ifndef EBUS_MINIMAL_DIAGNOSTICS
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
#endif
}

void BusMonitor::fetchUtilizationHistory(
    [[maybe_unused]] const std::function<void(float)>& callback) const {
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (callback) {
    utilization_history_.forEach(callback);
  }
#endif
}

void BusMonitor::logHandlerTransition([[maybe_unused]] HandlerState from,
                                      [[maybe_unused]] HandlerState to) {
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_history_.push_back({from, to, getNowMs()});
#endif
}

void BusMonitor::logRequestTransition([[maybe_unused]] RequestState from,
                                      [[maybe_unused]] RequestState to) {
#ifndef EBUS_MINIMAL_DIAGNOSTICS
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  request_history_.push_back({from, to, getNowMs()});
#endif
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

void BusMonitor::recordHandlerError(uint8_t address) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    Clock::now() - uptime_start_)
                    .count();

  handler_acc_.last_error_address = address;
  auto& top = handler_acc_.top_errors;

  int min_idx = 0;
  bool found = false;
  for (int i = 0; i < 3; ++i) {
    if (top[i].address == address) {
      top[i].count++;
      top[i].last_seen_us = now_us;
      found = true;
      break;
    }
    if (top[i].count < top[min_idx].count) min_idx = i;
  }

  if (!found) {
    top[min_idx] = {address, 1, static_cast<uint64_t>(now_us)};
  }

  std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) {
    if (a.count != b.count) return a.count > b.count;
    return a.last_seen_us > b.last_seen_us;
  });
}

void BusMonitor::recordHandlerSuccess(uint8_t address) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_acc_.last_success_address = address;
}

void BusMonitor::logPassiveReset() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_acc_.last_passive_reset_us =
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                            uptime_start_)
          .count();
  handler_acc_.resets_passive++;
}

void BusMonitor::logActiveReset() {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  handler_acc_.last_active_reset_us =
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                            uptime_start_)
          .count();
  handler_acc_.resets_active++;
}

void BusMonitor::clearHistory() {
#ifndef EBUS_MINIMAL_DIAGNOSTICS
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
#endif
}

}  // namespace ebus::detail

namespace ebus {

// --- Metrics Implementations ---

void MetricValues::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  float mean_us = (count > 0) ? static_cast<float>(sum_us) / count : 0.0f;
  writer.writeField("last_us", static_cast<uint64_t>(last_us));
  writer.writeField("max_us", static_cast<uint64_t>(max_us));
  writer.writeFieldFloat("mean_us", mean_us);
  writer.writeField("count", static_cast<uint64_t>(count));
  writer.endObject();
}

void metrics::HandlerMetrics::reset() {
  messages_passive = 0;
  messages_reactive = 0;
  messages_active = 0;
  error_passive = 0;
  error_reactive = 0;
  error_active = 0;
  resets_passive = 0;
  resets_active = 0;
  total_sent_data_bytes = 0;
  total_sent_protocol_bytes = 0;
  total_observed_data_bytes = 0;
  total_observed_protocol_bytes = 0;
  last_error_address = 0xff;
  last_success_address = 0xff;
  last_passive_reset_us = 0;
  last_active_reset_us = 0;
  top_errors.fill({});

  sync = {};
  write = {};
  passive_first = {};
  passive_data = {};
  active_first = {};
  active_data = {};
}

void metrics::HandlerMetrics::toJson(const JsonChunkVisitor& visitor) const {
  uint32_t m_total = messages_passive + messages_active + messages_reactive;
  uint32_t e_total = error_passive + error_reactive + error_active;

  float e_rate =
      (m_total > 0 || e_total > 0)
          ? (static_cast<float>(e_total) / (m_total + e_total)) * 100.0f
          : 0.0f;

  float pd_util = (total_sent_protocol_bytes > 0)
                      ? (static_cast<float>(total_sent_data_bytes) /
                         total_sent_protocol_bytes) *
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
  writer.writeField("messages_total", m_total);
  writer.writeField("messages_passive", messages_passive);
  writer.writeField("messages_reactive", messages_reactive);
  writer.writeField("messages_active", messages_active);
  writer.writeField("error_total", e_total);
  writer.writeField("error_passive", error_passive);
  writer.writeField("error_reactive", error_reactive);
  writer.writeField("error_active", error_active);
  writer.writeField("resets_passive", resets_passive);
  writer.writeField("resets_active", resets_active);
  writer.writeHexField("last_error_address", ByteView(&last_error_address, 1));
  if (last_success_address != 0xff) {
    writer.writeHexField("last_success_address",
                         ByteView(&last_success_address, 1));
  }
  if (last_passive_reset_us > 0) {
    writer.writeField("last_passive_reset_us", last_passive_reset_us);
  }
  if (last_active_reset_us > 0) {
    writer.writeField("last_active_reset_us", last_active_reset_us);
  }

  writer.appendKey("top_errors");
  writer.startArray();
  for (const auto& entry : top_errors) {
    if (entry.count > 0) {
      writer.startObject();
      writer.writeHexField("address", ByteView(&entry.address, 1));
      writer.writeField("count", static_cast<uint64_t>(entry.count));
      writer.writeField("last_seen_us", entry.last_seen_us);
      writer.endObject();
    }
  }
  writer.endArray();

  writer.writeField("total_sent_data_bytes", total_sent_data_bytes);
  writer.writeField("total_sent_protocol_bytes", total_sent_protocol_bytes);
  writer.writeField("total_observed_data_bytes", total_observed_data_bytes);
  writer.writeField("total_observed_protocol_bytes",
                    total_observed_protocol_bytes);

  writer.writeField("sync", sync);
  writer.writeField("write", write);
  writer.writeField("passive_first", passive_first);
  writer.writeField("passive_data", passive_data);
  writer.writeField("active_first", active_first);
  writer.writeField("active_data", active_data);
  writer.endObject();
}

void metrics::RequestMetrics::reset() {
  won_total = 0;
  lost_total = 0;
  collisions = 0;
  arbitration_errors = 0;
  first_syn = 0;
  bus_request_blocked = 0;
  lock_counter_reset = 0;
  session_timeouts = 0;
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
  writer.writeField("won_total", won_total);
  writer.writeField("lost_total", lost_total);
  writer.writeField("collisions", collisions);
  writer.writeField("arbitration_errors", arbitration_errors);
  writer.writeField("first_syn", first_syn);
  writer.endObject();
}

void metrics::BusMetrics::reset() {
  start_bit_errors = 0;
  syn_postponed_count = 0;
  congestion = false;
  high_jitter = false;
  last_error_us = 0;
  uptime_us = 0;

  delay = {};
  window = {};
  transmit = {};
  syn_postpone = {};
}

void metrics::BusMetrics::toJson(const JsonChunkVisitor& visitor) const {
  // Note: we'd need total_low_bits_ here for perfect accuracy, but uptime_us
  // and the other timing samples provide sufficient operational context.
  // BusMonitor::getBusUtilization() provides the real-time calculated value.
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("utilization", (const char*)nullptr);
  writer.writeField("start_bit_errors", start_bit_errors);
  writer.writeField("syn_postponed_count", syn_postponed_count);
  writer.writeField("congestion", congestion);
  writer.writeField("high_jitter", high_jitter);
  writer.writeField("last_error_us", last_error_us);
  writer.writeField("uptime_us", uptime_us);

  writer.writeField("delay", delay);
  writer.writeField("window", window);
  writer.writeField("transmit", transmit);
  writer.writeField("syn_postpone", syn_postpone);
  writer.endObject();
}

void metrics::DeviceMetrics::reset() { unknown_devices = 0; }

void metrics::DeviceMetrics::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("unknown_devices", unknown_devices);
  writer.endObject();
}

void metrics::ControllerMetrics::reset() { event_queue_dropped = 0; }

void metrics::ControllerMetrics::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("event_queue_dropped", event_queue_dropped);
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
  writer.writeValue(handler);
  writer.appendKey("request");
  writer.writeValue(request);
  writer.appendKey("bus");
  writer.writeValue(bus);
  writer.appendKey("devices");
  writer.writeValue(devices);
  writer.appendKey("controller");
  writer.writeValue(controller);
  writer.writeFieldFloat("quality", quality);
  writer.endObject();
}

// --- Status Implementations ---

void ThreadStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  if (name.empty()) {
    writer.writeField("name", "unknown");
  } else {
    writer.writeField("name", name);
  }

  // Only output stack metrics if they are actually available (ESP32)
  if (task_stack_bytes != -1) {
    writer.writeField("stack_size", static_cast<int64_t>(task_stack_bytes));
    writer.writeField("stack_free",
                      static_cast<int64_t>(task_stack_free_bytes));
  }
  writer.endObject();
}

void ControllerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("thread", thread);
  writer.endObject();
}

void BusStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("bus_thread", bus_thread);
  if (!syn_thread.name.empty()) {
    writer.writeField("syn_thread", syn_thread);
  }
  writer.endObject();
}

void BusHandlerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("thread", thread);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.writeField("max_queue_size", static_cast<uint64_t>(max_queue_size));
  writer.endObject();
}

void SchedulerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("thread", thread);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.writeField("max_queue_size", static_cast<uint64_t>(max_queue_size));
  writer.endObject();
}

void ClientManagerStatus::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("thread", thread);
  writer.writeField("queue_size", static_cast<uint64_t>(queue_size));
  writer.writeField("queue_capacity", static_cast<uint64_t>(queue_capacity));
  writer.writeField("max_queue_size", static_cast<uint64_t>(max_queue_size));
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
  writer.writeField("max_size", static_cast<uint64_t>(max_size));
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
                            detail::BusMonitor* monitor,
                            [[maybe_unused]] bool reset_histories) {
  if (!visitor) return;

  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("last_update_timestamp_ms",
                    status.last_update_timestamp_ms);
  writer.writeField("controller", status.controller);
  writer.writeField("bus", status.bus);
  writer.writeField("bus_handler", status.bus_handler);
  writer.writeField("scheduler", status.scheduler);
  writer.writeField("client_manager", status.client_manager);
  writer.writeField("device_manager", status.device_manager);
  writer.writeField("device_scanner", status.device_scanner);
  writer.writeField("poll_manager", status.poll_manager);

  if (monitor) {
#ifndef EBUS_MINIMAL_DIAGNOSTICS
    monitor->fetchHistory([&](const auto& h_hist, const auto& r_hist,
                              const auto& u_hist) {
      writer.appendKey("handler_history");
      writer.startArray();
      h_hist.forEach([&](const HandlerTransition& t) { writer.writeValue(t); });
      writer.endArray();

      writer.appendKey("request_history");
      writer.startArray();
      r_hist.forEach([&](const RequestTransition& t) { writer.writeValue(t); });
      writer.endArray();

      writer.appendKey("utilization_history");
      writer.startArray();
      u_hist.forEach([&](float util) { writer.writeValueFloat(util); });
      writer.endArray();
    });

    if (reset_histories) {
      monitor->clearHistory();
    }
#endif
  }
  writer.endObject();
}

}  // namespace ebus