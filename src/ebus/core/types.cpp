/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <charconv>
#include <ebus/detail/json_writer.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>

namespace ebus {

const char* toString(TelegramType type) noexcept {
  switch (type) {
    case TelegramType::broadcast:
      return "broadcast";
    case TelegramType::master_master:
      return "master_master";
    case TelegramType::master_slave:
      return "master_slave";
    default:
      return "unknown type";
  }
}

const char* toString(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::none:
      return "none";
    case LogLevel::error:
      return "error";
    case LogLevel::info:
      return "info";
    case LogLevel::debug:
      return "debug";
    default:
      return "unknown level";
  }
}

const char* toString(MessageType type) noexcept {
  switch (type) {
    case MessageType::active:
      return "active";
    case MessageType::passive:
      return "passive";
    case MessageType::reactive:
      return "reactive";
    default:
      return "unknown type";
  }
}

const char* toString(SequenceState state) noexcept {
  switch (state) {
    case SequenceState::seq_empty:
      return "sequence is empty";
    case SequenceState::seq_ok:
      return "sequence is ok";
    case SequenceState::err_seq_too_short:
      return "sequence is too short";
    case SequenceState::err_seq_too_long:
      return "sequence is too long";
    case SequenceState::err_source_address:
      return "source address is invalid";
    case SequenceState::err_target_address:
      return "target address is invalid";
    case SequenceState::err_data_byte:
      return "data byte is invalid";
    case SequenceState::err_crc_invalid:
      return "CRC byte is invalid";
    case SequenceState::err_ack_invalid:
      return "acknowledge byte is invalid";
    case SequenceState::err_ack_missing:
      return "acknowledge byte is missing";
    case SequenceState::err_ack_negative:
      return "acknowledge byte is negative";
    default:
      return "unknown state";
  }
}

const char* toString(HandlerState state) noexcept {
  switch (state) {
    case HandlerState::passive_receive_master:
      return "passive_receive_master";
    case HandlerState::passive_receive_master_acknowledge:
      return "passive_receive_master_acknowledge";
    case HandlerState::passive_receive_slave:
      return "passive_receive_slave";
    case HandlerState::passive_receive_slave_acknowledge:
      return "passive_receive_slave_acknowledge";
    case HandlerState::reactive_send_master_positive_acknowledge:
      return "reactive_send_master_positive_acknowledge";
    case HandlerState::reactive_send_master_negative_acknowledge:
      return "reactive_send_master_negative_acknowledge";
    case HandlerState::reactive_send_slave:
      return "reactive_send_slave";
    case HandlerState::reactive_receive_slave_acknowledge:
      return "reactive_receive_slave_acknowledge";
    case HandlerState::request_bus:
      return "request_bus";
    case HandlerState::active_send_master:
      return "active_send_master";
    case HandlerState::active_receive_master_acknowledge:
      return "active_receive_master_acknowledge";
    case HandlerState::active_receive_slave:
      return "active_receive_slave";
    case HandlerState::active_send_slave_positive_acknowledge:
      return "active_send_slave_positive_acknowledge";
    case HandlerState::active_send_slave_negative_acknowledge:
      return "active_send_slave_negative_acknowledge";
    case HandlerState::release_bus:
      return "release_bus";
    default:
      return "unknown_state";
  }
}

const char* toString(RequestState state) noexcept {
  switch (state) {
    case RequestState::observe:
      return "observe";
    case RequestState::first:
      return "first";
    case RequestState::retry:
      return "retry";
    case RequestState::second:
      return "second";
    default:
      return "unknown state";
  }
}

const char* toString(RequestResult state) noexcept {
  switch (state) {
    case RequestResult::observe_syn:
      return "observe_syn";
    case RequestResult::observe_data:
      return "observe_data";
    case RequestResult::first_syn:
      return "first_syn";
    case RequestResult::first_won:
      return "first_won";
    case RequestResult::first_retry:
      return "first_retry";
    case RequestResult::first_lost:
      return "first_lost";
    case RequestResult::first_error:
      return "first_error";
    case RequestResult::retry_syn:
      return "retry_syn";
    case RequestResult::retry_error:
      return "retry_error";
    case RequestResult::second_won:
      return "second_won";
    case RequestResult::second_lost:
      return "second_lost";
    case RequestResult::second_error:
      return "second_error";
    default:
      return "unknown state";
  }
}

const char* toString(ProtocolError error) noexcept {
  switch (error) {
    case ProtocolError::none:
      return "none";
    case ProtocolError::error_passive_master:
      return "Passive master error";
    case ProtocolError::error_passive_master_ack:
      return "Passive master ACK error";
    case ProtocolError::error_passive_slave:
      return "Passive slave error";
    case ProtocolError::error_passive_slave_ack:
      return "Passive slave ACK error";
    case ProtocolError::error_reactive_master:
      return "Reactive master error";
    case ProtocolError::error_reactive_master_ack:
      return "Reactive master ACK error";
    case ProtocolError::error_reactive_slave:
      return "Reactive slave error";
    case ProtocolError::error_reactive_slave_ack:
      return "Reactive slave ACK error";
    case ProtocolError::error_active_master_echo:
      return "Active master echo error";
    case ProtocolError::error_active_master:
      return "Active master error";
    case ProtocolError::error_active_master_ack:
      return "Active master ACK error";
    case ProtocolError::error_active_slave:
      return "Active slave error";
    case ProtocolError::error_active_slave_ack:
      return "Active slave ACK error";
    case ProtocolError::check_passive_buffers:
      return "Passive buffers check failed";
    case ProtocolError::check_active_buffers:
      return "Active buffers check failed";
    case ProtocolError::illegal_fsm_transition:
      return "Illegal FSM Transition";
    case ProtocolError::handler_busy:
      return "Handler busy";
    case ProtocolError::invalid_message:
      return "Invalid message";
    case ProtocolError::fsm_timeout:
      return "FSM timeout";
    case ProtocolError::arbitration_lost:
      return "Arbitration lost";
    case ProtocolError::total_transfer_timeout:
      return "Total transfer timeout";
    default:
      return "Unknown protocol error";
  }
}

const char* toString(ClientType type) noexcept {
  switch (type) {
    case ClientType::read_only:
      return "read_only";
    case ClientType::regular:
      return "regular";
    case ClientType::enhanced:
      return "enhanced";
    default:
      return "unknown type";
  }
}

const char* toString(SessionState state) noexcept {
  switch (state) {
    case SessionState::idle:
      return "idle";
    case SessionState::request:
      return "request";
    case SessionState::response:
      return "response";
    case SessionState::transmit:
      return "transmit";
    default:
      return "unknown state";
  }
}

void HandlerTransition::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeField("from", ebus::toString(from));
  writer.writeField("to", ebus::toString(to));
  writer.writeTimestampField("timestamp", timestamp);
}

void RequestTransition::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeField("from", ebus::toString(from));
  writer.writeField("to", ebus::toString(to));
  writer.writeTimestampField("timestamp", timestamp);
}

std::string ErrorEntry::toString() const {
  std::string res;
  res.reserve(128);  // Pre-allocate to avoid reallocations
  res += "[";

  char buf[12];
  if (poll_id > 0) {
    res += "P:";
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), poll_id);
    res.append(buf, static_cast<size_t>(ptr - buf));
    res += "|";
  }

  res += "S:";
  auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), session_id);
  res.append(buf, static_cast<size_t>(ptr - buf));

  res += "][";
  res += ebus::toString(handler_state);
  res += "][";
  res += ebus::toString(request_state);
  res += "] ";
  res += ebus::toString(protocol_error);

  if (sequence_state != SequenceState::seq_ok &&
      sequence_state != SequenceState::seq_empty) {
    res += " (";
    res += ebus::toString(sequence_state);
    res += ")";
  }

  res += " (Result: ";
  res += ebus::toString(result);
  res += ")";

  return res;
}

void ErrorEntry::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeField("session_id", session_id);
  writer.writeField("poll_id", poll_id);
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
}

}  // namespace ebus