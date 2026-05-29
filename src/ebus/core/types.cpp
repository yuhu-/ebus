/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/types.hpp>
#include <ebus/detail/json_writer.hpp> // For detail::JsonWriter
#include <ebus/utils.hpp>

namespace ebus {

void HandlerTransition::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("from", ebus::toString(from));
  writer.writeField("to", ebus::toString(to));
  writer.writeTimestampField("timestamp", timestamp);
  writer.endObject();
}

void RequestTransition::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("from", ebus::toString(from));
  writer.writeField("to", ebus::toString(to));
  writer.writeTimestampField("timestamp", timestamp);
  writer.endObject();
}

void ErrorEntry::toJson(const JsonChunkVisitor& visitor) const {
  detail::JsonWriter writer(visitor);
  writer.startObject();
  writer.writeField("session_id", static_cast<uint64_t>(session_id));
  writer.writeField("poll_id", static_cast<uint64_t>(poll_id));
  writer.writeField("level", ebus::toString(level));
  writer.writeField("protocol_error", ebus::toString(protocol_error));
  writer.writeField("result", ebus::toString(result));
  writer.writeField("sequence_state", ebus::toString(sequence_state));
  writer.writeField("handler_state", ebus::toString(handler_state));
  writer.writeField("request_state", ebus::toString(request_state));
  writer.writeHexField("master", master);
  writer.writeHexField("slave", slave);

  char iso_buffer[26];
  ebus::formatIso8601Fast(timestamp, iso_buffer);
  writer.writeField("timestamp", std::string_view(iso_buffer));

  writer.endObject();
}

}  // namespace ebus