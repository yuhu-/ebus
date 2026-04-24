/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace ebus {

// --- Protocol Enums ---
enum class TelegramType { undefined, broadcast, master_master, master_slave };
enum class MessageType { undefined, active, passive, reactive };

enum class LogLevel { none, error, info, debug };

/**
 * Available client types for the network bridge.
 */
enum class ClientType { read_only, regular, enhanced };

enum class BridgeAction { keep_active, stop_session };

enum class SequenceState {
  seq_empty,
  seq_ok,
  err_seq_too_short,
  err_seq_too_long,
  err_source_address,
  err_target_address,
  err_data_byte,
  err_crc_invalid,
  err_ack_invalid,
  err_ack_missing,
  err_ack_negative
};

enum class HandlerState {
  passive_receive_master,
  passive_receive_master_acknowledge,
  passive_receive_slave,
  passive_receive_slave_acknowledge,
  reactive_send_master_positive_acknowledge,
  reactive_send_master_negative_acknowledge,
  reactive_send_slave,
  reactive_receive_slave_acknowledge,
  request_bus,
  active_send_master,
  active_receive_master_acknowledge,
  active_receive_slave,
  active_send_slave_positive_acknowledge,
  active_send_slave_negative_acknowledge,
  release_bus
};

enum class RequestState { observe, first, retry, second };

enum class RequestResult {
  observe_syn,
  observe_data,
  first_syn,
  first_won,
  first_retry,
  first_lost,
  first_error,
  retry_syn,
  retry_error,
  second_won,
  second_lost,
  second_error
};

// --- String Conversion ---

constexpr const char* toString(SequenceState state) noexcept {
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

constexpr const char* toString(HandlerState state) noexcept {
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

constexpr const char* toString(RequestState state) noexcept {
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

constexpr const char* toString(RequestResult state) noexcept {
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

}  // namespace ebus
