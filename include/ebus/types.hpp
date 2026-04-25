/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ebus {

// --- Physical Constants ---
constexpr double bit_time_us = 1000000.0 / 2400.0;  // ~416.67 us

// --- Protocol Constants ---
struct Protocol {
  static constexpr uint8_t sym_zero = 0x00;     // zero byte
  static constexpr uint8_t sym_syn = 0xaa;      // synchronization byte
  static constexpr uint8_t sym_ext = 0xa9;      // extend byte
  static constexpr uint8_t sym_syn_ext = 0x01;  // extended synchronization byte
  static constexpr uint8_t sym_ext_ext = 0x00;  // extended extend byte
  static constexpr uint8_t sym_ack = 0x00;      // positive acknowledge
  static constexpr uint8_t sym_nak = 0xff;      // negative acknowledge
  static constexpr uint8_t sym_broad = 0xfe;    // broadcast destination address

  /**
   * Returns true if the byte requires eBUS stuffing (0xAA or 0xA9).
   */
  static constexpr bool needsEscape(uint8_t byte) {
    return byte == sym_syn || byte == sym_ext;
  }

  /**
   * Escapes a byte according to eBUS rules (Spec 5.1).
   * out[0] is the escape byte (0xA9), out[1] is the escaped value.
   */
  static constexpr void escape(uint8_t byte, uint8_t out[2]) {
    out[0] = sym_ext;
    out[1] = (byte == sym_syn) ? sym_syn_ext : sym_ext_ext;
  }

  /**
   * Attempts to unescape an eBUS sequence.
   * Returns true if the sequence was a valid escape sequence.
   */
  static constexpr bool unescape(uint8_t b1, uint8_t b2, uint8_t& out) {
    if (b1 != sym_ext) return false;
    if (b2 == sym_syn_ext) {
      out = sym_syn;
      return true;
    }
    if (b2 == sym_ext_ext) {
      out = sym_ext;
      return true;
    }
    return false;
  }
};

// --- Protocol Limits ---
namespace limits {
static constexpr uint8_t max_data_bytes = 16;  // Maximum data bytes (Spec 5.6)
static constexpr uint8_t max_telegram_bytes =
    48;  // Safe upper bound for MS telegrams
static constexpr uint8_t max_lock_counter = 25;
static constexpr uint16_t min_window = 4000;
static constexpr uint16_t max_window = 5000;
static constexpr uint16_t max_offset = 500;
static constexpr double byte_center_bits = 9.5;  // midpoint of the stop bit
}  // namespace limits

constexpr size_t default_sequence_capacity = 64;

// --- Number FSM States ---
constexpr size_t num_handler_states = 15;
constexpr size_t num_request_states = 4;

// --- Defaults ---
namespace defaults {

constexpr uint8_t address = 0xff;

struct Arbitration {                          // Arbitration defaults
  static constexpr uint8_t lock_counter = 3;  // Default lock counter
};

struct Bus {
  static constexpr uint16_t window = 4300;  // us
  static constexpr uint16_t offset = 80;    // us
  static constexpr uint32_t baud_rate = 2400;
  static constexpr const char* device_path = "/dev/ttyUSB0";
  struct Syn {
    static constexpr uint32_t base_ms = 50;
    static constexpr uint32_t tolerance_ms = 5;
  };
};

struct Scheduler {
  static constexpr int max_send_attempts = 3;
  static constexpr uint32_t base_backoff_ms = 100;
  static constexpr uint32_t fsm_timeout_ms = 2000;
  static constexpr uint32_t total_timeout_ms = 4000;
};

struct Network {
  static constexpr uint32_t client_timeout_ms = 1000;
  static constexpr uint32_t watchdog_timeout_ms = 5000;
  static constexpr size_t outbound_buffer_size = 4096;
};

struct Logging {
  static constexpr size_t log_size = 10;
};

}  // namespace defaults

static_assert(defaults::Bus::offset < limits::min_window,
              "The default offset must be smaller than the minimum arbitration "
              "window to prevent timing underflow.");

// --- enums ---

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

// --- Addressing logic ---

/**
 * Checks if a byte conforms to the eBUS master address rules (Spec 6.2.2.1).
 */
constexpr bool isMaster(uint8_t byte) {
  return ((((byte >> 4) & 0x0f) + 1) & ((byte >> 4) & 0x0f)) == 0 &&
         (((byte & 0x0f) + 1) & (byte & 0x0f)) == 0;
}

constexpr bool isSlave(uint8_t byte) {
  return !isMaster(byte) && byte != Protocol::sym_syn &&
         byte != Protocol::sym_ext && byte != Protocol::sym_broad;
}

constexpr bool isTarget(uint8_t byte) {
  return byte != Protocol::sym_syn && byte != Protocol::sym_ext;
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
  if (byte == Protocol::sym_broad) return TelegramType::broadcast;
  if (isMaster(byte)) return TelegramType::master_master;
  return TelegramType::master_slave;
}

/**
 * Persistent entry for the diagnostic error log.
 * Uses fixed-size buffers to ensure zero heap allocation during logging.
 */
struct ErrorEntry {
  LogLevel level;
  char message[48];  // Sufficient for protocol error literals
  RequestResult result;
  HandlerState handler_state;
  RequestState request_state;
  uint8_t master[32];
  uint8_t master_len;
  uint8_t slave[32];
  uint8_t slave_len;
  double utilization;
  std::chrono::system_clock::time_point timestamp;

  // Custom stringifier for human-readable logs
  std::string toString() const {
    return "[" + std::string(ebus::toString(handler_state)) + "][" +
           ebus::toString(request_state) + "] " + message +
           " (Result: " + ebus::toString(result) + ")";
  }

  void setMessage(std::string_view msg) {
    size_t len = std::min(msg.size(), sizeof(message) - 1);
    std::memcpy(message, msg.data(), len);
    message[len] = '\0';
  }

  void setMaster(const uint8_t* data, size_t size) {
    master_len = static_cast<uint8_t>(std::min(size, sizeof(master)));
    if (master_len > 0) std::memcpy(master, data, master_len);
  }

  void setSlave(const uint8_t* data, size_t size) {
    slave_len = static_cast<uint8_t>(std::min(size, sizeof(slave)));
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

/**
 * A lightweight, non-owning view of a byte sequence.
 * Similar to std::string_view but for uint8_t.
 */
struct ByteView {
  constexpr ByteView() = default;
  constexpr ByteView(const uint8_t* data, size_t size)
      : data_(data), size_(size) {}

  // Implicit conversion from std::vector is intentional to allow transparent
  // usage of owning containers in functions accepting views.
  ByteView(const std::vector<uint8_t>& v) : data_(v.data()), size_(v.size()) {}

  constexpr const uint8_t* data() const noexcept { return data_; }
  constexpr size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr const uint8_t* begin() const noexcept { return data_; }
  constexpr const uint8_t* end() const noexcept { return data_ + size_; }

  constexpr uint8_t operator[](size_t i) const { return data_[i]; }

  bool operator==(ByteView other) const {
    if (this == &other) return true;
    if (size_ != other.size_) return false;
    return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
  }
  bool operator!=(ByteView other) const { return !(*this == other); }

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
};

// Ensure definitions.hpp remains lean and free of heavy template logic
static_assert(std::is_standard_layout_v<ByteView>,
              "ByteView must maintain standard layout for ABI compatibility.");
static_assert(std::is_trivially_copyable_v<ByteView>,
              "ByteView must be trivially copyable to remain heap-free in the "
              "hot path.");

}  // namespace ebus
