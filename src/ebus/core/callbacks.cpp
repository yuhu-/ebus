/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/callbacks.hpp>
#include <ebus/sequence.hpp>

#include "utils/json_utils.hpp"

namespace ebus {

void TelegramInfo::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "session_id", static_cast<uint64_t>(session_id),
               first_field);
  append_field(json, "poll_id", static_cast<uint64_t>(poll_id), first_field);
  append_field(json, "retry_count", static_cast<uint64_t>(retry_count),
               first_field);
  append_enum_field(json, "message_type", message_type, first_field);
  append_enum_field(json, "telegram_type", telegram_type, first_field);
  append_enum_field(json, "handler_state", handler_state, first_field);
  append_enum_field(json, "request_state", request_state, first_field);
  append_hex_field(json, "master", master_view, first_field);
  append_hex_field(json, "slave", slave_view, first_field);
  json += "}";
}

void ErrorInfo::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "session_id", static_cast<uint64_t>(session_id),
               first_field);
  append_field(json, "poll_id", static_cast<uint64_t>(poll_id), first_field);
  append_enum_field(json, "level", level, first_field);
  append_enum_field(json, "protocol_error", protocol_error, first_field);
  append_enum_field(json, "result", result, first_field);
  append_enum_field(json, "sequence_state", sequence_state, first_field);
  append_enum_field(json, "handler_state", handler_state, first_field);
  append_enum_field(json, "request_state", request_state, first_field);
  append_hex_field(json, "master", master_view, first_field);
  append_hex_field(json, "slave", slave_view, first_field);
  json += "}";
}

void ReactiveInfo::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "session_id", static_cast<uint64_t>(session_id),
               first_field);
  append_hex_field(json, "master", master_view, first_field);
  append_field(json, "slave_response", slave_response.toString(), first_field);
  json += "}";
}

void ResultInfo::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_field(json, "session_id", static_cast<uint64_t>(session_id),
               first_field);
  append_field(json, "poll_id", static_cast<uint64_t>(poll_id), first_field);
  append_field(json, "success", success, first_field);
  append_enum_field(json, "result", result, first_field);
  append_enum_field(json, "sequence_state", sequence_state, first_field);
  append_hex_field(json, "master", master_view, first_field);
  append_hex_field(json, "slave", slave_view, first_field);
  json += "}";
}

void BusEventInfo::toJson(std::string& json) const {
  // Convert steady_clock to system_clock (approximation for external logs)
  auto wall_time =
      std::chrono::system_clock::now() +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          timestamp - Clock::now());
  time_t t = std::chrono::system_clock::to_time_t(wall_time);

  json += "{";
  bool first_field = true;  // Reset for this function
  append_hex_field(json, "byte", ByteView(&byte, 1), first_field);
  append_enum_field(json, "handler_state", handler_state, first_field);
  append_enum_field(json, "request_state", request_state, first_field);
  append_enum_field(json, "result", result, first_field);
  append_field(json, "lock_counter", static_cast<int64_t>(lock_counter),
               first_field);

  // Manual timestamp formatting for milliseconds
  if (!first_field) {
    json += ",";
  }
  json += "\"timestamp\":\"";
  struct tm tm_info;
  gmtime_r(&t, &tm_info);
  char time_buffer[32];  // YYYY-MM-DDTHH:MM:SS.mmmZ
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S", &tm_info);
  json += time_buffer;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                wall_time.time_since_epoch())
                .count() %
            1000;
  char ms_buffer[8];
  snprintf(ms_buffer, sizeof(ms_buffer), ".%03lldZ", (long long)ms);
  json += ms_buffer;
  json += "\"";
  first_field = false;

  json += "}";
}

}  // namespace ebus