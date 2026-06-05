/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/callbacks.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>

namespace ebus {

void TelegramInfo::toJson(detail::JsonWriter& writer) const {
  writer.startObject();
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
  writer.writeField("retry_count", retry_count);
  writer.writeField("message_type", toString(message_type));
  writer.writeField("telegram_type", toString(telegram_type));
  writer.writeField("handler_state", toString(handler_state));
  writer.writeField("request_state", toString(request_state));
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave", slave_view);
  writer.endObject();
}

void ErrorInfo::toJson(detail::JsonWriter& writer) const {
  writer.startObject();
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
  writer.writeField("level", toString(level));
  writer.writeField("protocol_error", toString(protocol_error));
  writer.writeField("result", toString(result));
  writer.writeField("sequence_state", toString(sequence_state));
  writer.writeField("handler_state", toString(handler_state));
  writer.writeField("request_state", toString(request_state));
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave", slave_view);
  writer.endObject();
}

void ReactiveInfo::toJson(detail::JsonWriter& writer) const {
  writer.startObject();
  writer.writeField("session_id", session_id);
  writer.writeHexField("master", master_view);
  writer.writeField("slave_response", slave_response.toString());
  writer.endObject();
}

void ResultInfo::toJson(detail::JsonWriter& writer) const {
  writer.startObject();
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
  writer.writeField("success", success);
  writer.writeField("result", toString(result));
  writer.writeField("sequence_state", toString(sequence_state));
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave", slave_view);
  writer.endObject();
}

void BusEventInfo::toJson(detail::JsonWriter& writer) const {
  // Convert steady_clock to system_clock (approximation for external logs)
  auto wall_time =
      std::chrono::system_clock::now() +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          timestamp - Clock::now());

  char iso_buffer[26];
  ebus::formatIso8601Fast(std::chrono::duration_cast<std::chrono::milliseconds>(
                              wall_time.time_since_epoch())
                              .count(),
                          iso_buffer);

  writer.startObject();
  writer.writeHexField("byte", ByteView(&byte, 1));
  writer.writeField("handler_state", ebus::toString(handler_state));
  writer.writeField("request_state", ebus::toString(request_state));
  writer.writeField("result", ebus::toString(result));
  writer.writeField("lock_counter", lock_counter);
  writer.writeField("timestamp", std::string_view(iso_buffer));
  writer.endObject();
}

}  // namespace ebus