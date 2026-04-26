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

namespace ebus {

/**
 * The source of truth for monotonic time within the library.
 * Using an alias allows for platform-specific overrides or
 * clock injection during unit testing.
 */
using Clock = std::chrono::steady_clock;

/**
 * Core eBUS byte symbols as defined in the specification.
 */
struct Symbols {
  static constexpr uint8_t zero = 0x00;     // zero byte
  static constexpr uint8_t syn = 0xaa;      // synchronization byte
  static constexpr uint8_t ext = 0xa9;      // extend byte
  static constexpr uint8_t syn_ext = 0x01;  // extended synchronization byte
  static constexpr uint8_t ext_ext = 0x00;  // extended extend byte
  static constexpr uint8_t ack = 0x00;      // positive acknowledge
  static constexpr uint8_t nak = 0xff;      // negative acknowledge
  static constexpr uint8_t broad = 0xfe;    // broadcast destination address

  /**
   * Returns true if the byte requires eBUS stuffing (0xAA or 0xA9).
   */
  static constexpr bool needsEscape(uint8_t byte) {
    return byte == syn || byte == ext;
  }

  /**
   * Escapes a byte according to eBUS rules (Spec 5.1).
   * out[0] is the escape byte (0xA9), out[1] is the escaped value.
   */
  static constexpr void escape(uint8_t byte, uint8_t out[2]) {
    out[0] = ext;
    out[1] = (byte == syn) ? syn_ext : ext_ext;
  }

  /**
   * Attempts to unescape an eBUS sequence.
   * Returns true if the sequence was a valid escape sequence.
   */
  static constexpr bool unescape(uint8_t b1, uint8_t b2, uint8_t& out) {
    if (b1 != ext) return false;
    if (b2 == syn_ext) {
      out = syn;
      return true;
    }
    if (b2 == ext_ext) {
      out = ext;
      return true;
    }
    return false;
  }
};

// --- enums ---

enum class LogLevel { none, error, info, debug };

// --- Protocol Enums ---
enum class TelegramType { undefined, broadcast, master_master, master_slave };
enum class MessageType { undefined, active, passive, reactive };

/**
 * Available client types for the network bridge.
 */
enum class ClientType { read_only, regular, enhanced };

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

// --- Addressing logic ---

/**
 * Checks if a byte conforms to the eBUS master address rules (Spec 6.2.2.1).
 */
constexpr bool isMaster(uint8_t byte) {
  return ((((byte >> 4) & 0x0f) + 1) & ((byte >> 4) & 0x0f)) == 0 &&
         (((byte & 0x0f) + 1) & (byte & 0x0f)) == 0;
}

constexpr bool isSlave(uint8_t byte) {
  return !isMaster(byte) && byte != Symbols::syn && byte != Symbols::ext &&
         byte != Symbols::broad;
}

constexpr bool isTarget(uint8_t byte) {
  return byte != Symbols::syn && byte != Symbols::ext;
}

constexpr uint8_t masterOf(uint8_t byte) {
  if (byte == 0x04) return 0xff;
  if (byte < 5) return byte;
  uint8_t potential = static_cast<uint8_t>(byte - 5);
  return isMaster(potential) ? potential : byte;
}

constexpr uint8_t slaveOf(uint8_t byte) {
  if (byte == 0xff) return 0x04;
  if (!isMaster(byte)) return byte;
  uint8_t result = static_cast<uint8_t>(byte + 5);
  return isSlave(result) ? result : byte;
}

constexpr TelegramType typeOf(uint8_t byte) {
  if (byte == Symbols::broad) return TelegramType::broadcast;
  if (isMaster(byte)) return TelegramType::master_master;
  return TelegramType::master_slave;
}

/**
 * Persistent entry for the diagnostic error log.
 * Uses fixed-size buffers to ensure zero heap allocation during logging.
 */
struct ErrorEntry {
  LogLevel level;
  char message[64];  // Sufficient for protocol and vendor error literals
  RequestResult result;
  SequenceState sequence_state;
  HandlerState handler_state;
  RequestState request_state;
  uint8_t master[32];
  uint8_t master_len;
  uint8_t slave[32];
  uint8_t slave_len;
  double utilization;
  uint64_t timestamp;  // ms since epoch

  // Custom stringifier for human-readable logs
  std::string toString() const {
    std::string res = "[" + std::string(ebus::toString(handler_state)) + "][" +
                      ebus::toString(request_state) + "] " + message;
    if (sequence_state != SequenceState::seq_ok &&
        sequence_state != SequenceState::seq_empty) {
      res += " (" + std::string(ebus::toString(sequence_state)) + ")";
    }
    res += " (Result: " + std::string(ebus::toString(result)) + ")";
    return res;
  }

  void setMessage(std::string_view msg) {
    const size_t max_len = sizeof(message) - 1;
    size_t len = (msg.size() < max_len) ? msg.size() : max_len;
    std::memcpy(message, msg.data(), len);
    message[len] = '\0';
  }

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

/**
 * Serializes ErrorEntry to a JSON object string.
 */
std::string toJson(const ErrorEntry& entry);

/**
 * Serializes a vector of ErrorEntry to a JSON array string.
 */
std::string toJson(const std::vector<ErrorEntry>& errors);

}  // namespace ebus
