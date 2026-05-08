/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ctime>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/data_types.hpp>
#include <ebus/device.hpp>
#include <ebus/metrics.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ebus {

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
      << "\"syn\": {"
      << "\"enabled\": " << (bus.syn.enabled ? "true" : "false") << ","
      << "\"base_ms\": " << bus.syn.base_ms << ","
      << "\"tolerance_ms\": " << bus.syn.tolerance_ms << "}},"
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

  oss << "{"
      << "\"byte\":\"" << toString(byte) << "\","
      << "\"handler_state\":\"" << toString(handler_state) << "\","
      << "\"request_state\":\"" << toString(request_state) << "\","
      << "\"result\":\"" << toString(result) << "\","
      << "\"lock_counter\":" << static_cast<int>(lock_counter) << ","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S") << "."
      << std::setw(3) << std::setfill('0') << ms << "Z\""
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
  oss << "{"
      << "\"from\":\"" << toString(from) << "\","
      << "\"to\":\"" << toString(to) << "\","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&s), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

std::string RequestTransition::toJson() const {
  std::ostringstream oss;
  time_t s = static_cast<time_t>(timestamp / 1000);
  oss << "{"
      << "\"from\":\"" << toString(from) << "\","
      << "\"to\":\"" << toString(to) << "\","
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&s), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

std::string ErrorEntry::toJson() const {
  std::ostringstream oss;
  time_t t = static_cast<time_t>(timestamp / 1000);
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
      << "\"timestamp\":\""
      << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\""
      << "}";
  return oss.str();
}

// --- metrics.hpp ---

std::string MetricValues::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"last\":" << last << ",\"min\":" << min << ",\"max\":" << max
      << ",\"mean\":" << mean << ",\"stddev\":" << stddev
      << ",\"count\":" << count << "}";
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
  for (size_t i = 0; i < transition_history.size(); ++i) {
    if (i > 0) oss << ",";
    oss << transition_history[i].toJson();
  }
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
    oss << ",\"last_error_timestamp\":\""
        << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ") << "\"";
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

std::string metrics::SystemMetrics::toJson() const {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  oss << "{\"handler\":" << handler.toJson()
      << ",\"request\":" << request.toJson() << ",\"bus\":" << bus.toJson()
      << ",\"devices\":" << devices.toJson() << ",\"quality\":" << quality
      << "}";
  return oss.str();
}

std::string ThreadStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"stack_size\":" << task_stack_bytes << ","
      << "\"stack_free\":" << task_stack_free_bytes << "}";
  return oss.str();
}

std::string ScannerStatus::toJson() const {
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

std::string PollStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"item_count\":" << item_count << "}";
  return oss.str();
}

std::string ServiceStatus::toJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"free_heap\":" << free_heap_bytes << ","
      << "\"min_free_heap\":" << min_free_heap_bytes << ","
      << "\"services\":[";
  for (size_t i = 0; i < services.size(); ++i) {
    if (i > 0) oss << ",";
    const auto& e = services[i];
    oss << "{"
        << "\"name\":\"" << e.name << "\",";
    if (e.last_update_timestamp_ms > 0) {
      oss << "\"last_update_timestamp_ms\":" << e.last_update_timestamp_ms
          << ",";
    }
    oss << "\"thread\":" << e.thread.toJson() << ","
        << "\"queue_size\":" << e.queue_size << ","
        << "\"queue_capacity\":" << e.queue_capacity << "}";
  }
  oss << "],\"syn_generator_thread\":" << syn_generator_thread.toJson()
      << ",\"controller_thread\":" << controller_thread.toJson()
      << ",\"bus_thread\":" << bus_thread.toJson()
      << ",\"scanner\":" << scanner.toJson() << ",\"poll\":" << poll.toJson()
      << "}";
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
