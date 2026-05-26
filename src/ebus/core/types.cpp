/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/types.hpp>

#include "utils/json_utils.hpp"

namespace ebus {

void HandlerTransition::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_enum_field(json, "from", from, first_field);
  append_enum_field(json, "to", to, first_field);
  append_json_timestamp(json, "timestamp", timestamp, first_field);
  json += "}";
}

void RequestTransition::toJson(std::string& json) const {
  json += "{";
  bool first_field = true;
  append_enum_field(json, "from", from, first_field);
  append_enum_field(json, "to", to, first_field);
  append_json_timestamp(json, "timestamp", timestamp, first_field);
  json += "}";
}

void ErrorEntry::toJson(std::string& json) const {
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
  append_hex_field(json, "master", master, first_field);
  append_hex_field(json, "slave", slave, first_field);
  append_json_timestamp(json, "timestamp", timestamp, first_field);
  json += "}";
}

}  // namespace ebus