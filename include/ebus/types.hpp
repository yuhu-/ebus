/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "ebus/address.hpp"
#include "ebus/detail/protocol_limits.hpp"

namespace ebus {

/**
 * The source of truth for monotonic time within the library.
 * Using an alias allows for platform-specific overrides or
 * clock injection during unit testing.
 */
using Clock = std::chrono::steady_clock;

// --- Protocol Enums ---

enum class LogLevel { none, error, info, debug };

enum class MessageType { undefined, active, passive, reactive };

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

enum class ProtocolError {
  none,
  error_passive_master,
  error_passive_master_ack,
  error_passive_slave,
  error_passive_slave_ack,
  error_reactive_master,
  error_reactive_master_ack,
  error_reactive_slave,
  error_reactive_slave_ack,
  error_active_master_echo,
  error_active_master,
  error_active_master_ack,
  error_active_slave,
  error_active_slave_ack,
  check_passive_buffers,
  check_active_buffers,
  illegal_fsm_transition,
  handler_busy,
  invalid_message,
  fsm_timeout,
  arbitration_lost,
  total_transfer_timeout,
};

/**
 * Available client types for the network bridge.
 */
enum class ClientType { read_only, regular, enhanced };

enum class SessionState {
  idle,      // Waiting for a client to have data
  request,   // Bus request pending, waiting for our slot to send
  response,  // Waiting for arbitration result from eBUS
  transmit   // Arbitration won, sending telegram body
};

// --- String Conversion ---

constexpr const char* toString(LogLevel level) noexcept {
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

constexpr const char* toString(MessageType type) noexcept {
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

constexpr const char* toString(ProtocolError error) noexcept {
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

constexpr const char* toString(ClientType type) noexcept {
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

constexpr const char* toString(SessionState state) noexcept {
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

// --- Struct ---

/**
 * Records a single state transition in the protocol handler.
 */
struct HandlerTransition {
  HandlerState from;
  HandlerState to;
  uint64_t timestamp;  // ms since epoch

  std::string toJson() const;
};

/**
 * Records a single state transition in the arbitration engine.
 */
struct RequestTransition {
  RequestState from;
  RequestState to;
  uint64_t timestamp;  // ms since epoch

  std::string toJson() const;
};

/**
 * Persistent entry for the diagnostic error log.
 * Uses fixed-size buffers to ensure zero heap allocation during logging.
 */
struct ErrorEntry {
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  LogLevel level;
  ProtocolError protocol_error;
  RequestResult result;
  SequenceState sequence_state;
  HandlerState handler_state;
  RequestState request_state;
  uint8_t master[detail::SequenceLimits::default_capacity];
  uint8_t master_len;
  uint8_t slave[detail::SequenceLimits::default_capacity];
  uint8_t slave_len;
  float utilization;
  uint64_t timestamp;  // ms since epoch

  std::string toJson() const;

  // Custom stringifier for human-readable logs
  std::string toString() const {
    std::string res = "[";
    if (poll_id > 0) res += "P:" + std::to_string(poll_id) + "|";
    res += "S:" + std::to_string(session_id) + "][";
    res += std::string(ebus::toString(handler_state)) + "][" +
           ebus::toString(request_state) + "] " +
           ebus::toString(protocol_error);

    if (sequence_state != SequenceState::seq_ok &&
        sequence_state != SequenceState::seq_empty) {
      res += " (" + std::string(ebus::toString(sequence_state)) + ")";
    }

    res += " (Result: " + std::string(ebus::toString(result)) + ")";
    return res;
  }

  void setProtocolError(ProtocolError error) { protocol_error = error; }

  void setMaster(const uint8_t* data, size_t size) {
    const size_t max_len = sizeof(master);
    master_len = static_cast<uint8_t>((size < max_len) ? size : max_len);
    if (master_len > 0) std::memcpy(master, data, master_len);
  }

  void setSlave(const uint8_t* data, size_t size) {
    const size_t max_len = sizeof(slave);
    slave_len = static_cast<uint8_t>((size < max_len) ? size : max_len);
    if (slave_len > 0) std::memcpy(slave, data, slave_len);
  }
};

}  // namespace ebus
