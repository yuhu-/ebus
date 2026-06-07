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
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
  writer.writeField("retry_count", retry_count);
  writer.writeField("message_type", toString(message_type));
  writer.writeField("telegram_type", toString(telegram_type));
  writer.writeField("handler_state", toString(handler_state));
  writer.writeField("request_state", toString(request_state));
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave", slave_view);
}

void ErrorInfo::toJson(detail::JsonWriter& writer) const {
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
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
}

void ReactiveInfo::toJson(detail::JsonWriter& writer) const {
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
  writer.writeField("session_id", session_id);
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave_response", slave_response);
}

void ResultInfo::toJson(detail::JsonWriter& writer) const {
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
  writer.writeField("success", success);
  writer.writeField("result", toString(result));
  writer.writeField("sequence_state", toString(sequence_state));
  writer.writeHexField("master", master_view);
  writer.writeHexField("slave", slave_view);
}

void BusEventInfo::toJson(detail::JsonWriter& writer) const {
  // Convert steady_clock to system_clock (approximation for external logs)
  auto wall_time =
      std::chrono::system_clock::now() +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          timestamp - Clock::now());

  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
  writer.writeHexField("byte", ByteView(&byte, 1));
  writer.writeField("handler_state", ebus::toString(handler_state));
  writer.writeField("request_state", ebus::toString(request_state));
  writer.writeField("result", ebus::toString(result));
  writer.writeField("lock_counter", lock_counter);
  writer.writeTimestampField(
      "timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                       wall_time.time_since_epoch())
                       .count());
}

}  // namespace ebus