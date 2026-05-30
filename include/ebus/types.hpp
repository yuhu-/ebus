/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
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

/**
 * @brief Callback for streaming JSON chunks.
 */
using JsonChunkVisitor = std::function<void(std::string_view)>;

namespace detail {
class JsonWriter; // Forward declaration
}

// --- Protocol Enums ---

enum class LogLevel : uint8_t { none, error, info, debug };

enum class MessageType : uint8_t { undefined, active, passive, reactive };

enum class SequenceState : uint8_t {
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

enum class HandlerState : uint8_t {
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

enum class RequestState : uint8_t { observe, first, retry, second };

enum class RequestResult : uint8_t {
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

enum class ProtocolError : uint8_t {
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
enum class ClientType : uint8_t { read_only, regular, enhanced };

enum class SessionState : uint8_t {
  idle,      // Waiting for a client to have data
  request,   // Bus request pending, waiting for our slot to send
  response,  // Waiting for arbitration result from eBUS
  transmit   // Arbitration won, sending telegram body
};

// --- String Conversion ---

const char* toString(LogLevel level) noexcept;
const char* toString(MessageType type) noexcept;
const char* toString(SequenceState state) noexcept;
const char* toString(HandlerState state) noexcept;
const char* toString(RequestState state) noexcept;
const char* toString(RequestResult state) noexcept;
const char* toString(ProtocolError error) noexcept;
const char* toString(ClientType type) noexcept;
const char* toString(SessionState state) noexcept;

// --- Struct ---

/**
 * A lightweight, non-owning view of a byte sequence.
 * Similar to std::string_view but for uint8_t.
 */
struct ByteView {
  constexpr ByteView() = default;
  constexpr ByteView(const uint8_t* data, size_t size)
      : data_(data), size_(size) {}

  ByteView(const std::vector<uint8_t>& v) : data_(v.data()), size_(v.size()) {}

  constexpr const uint8_t* begin() const noexcept { return data_; }
  constexpr const uint8_t* end() const noexcept { return data_ + size_; }

  constexpr const uint8_t* data() const noexcept { return data_; }

  constexpr size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

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

/**
 * A trivially copyable, owning byte sequence for use in bitwise-copy queues.
 */
template <size_t Cap>
struct StaticSequence {
  uint8_t buffer[Cap]{};
  uint8_t size_bytes = 0;

  void assign(const uint8_t* data, size_t len) {
    size_bytes = static_cast<uint8_t>((len < Cap) ? len : Cap);
    if (size_bytes > 0) std::memcpy(buffer, data, size_bytes);
  }

  uint8_t* begin() noexcept { return buffer; }
  const uint8_t* begin() const noexcept { return buffer; }
  uint8_t* end() noexcept { return buffer + size_bytes; }
  const uint8_t* end() const noexcept { return buffer + size_bytes; }

  uint8_t* data() noexcept { return buffer; }
  const uint8_t* data() const noexcept { return buffer; }

  size_t size() const noexcept { return size_bytes; }
  size_t capacity() const noexcept { return Cap; }
  void clear() noexcept { size_bytes = 0; }
  bool empty() const { return size_bytes == 0; }

  uint8_t& operator[](size_t i) { return buffer[i]; }
  const uint8_t& operator[](size_t i) const { return buffer[i]; }

  /**
   * Implicit conversion to ByteView for zero-copy interoperability.
   */
  operator ByteView() const { return ByteView(buffer, size_bytes); }
};

/**
 * Records a single state transition in the protocol handler.
 */
struct HandlerTransition {
  HandlerState from;
  HandlerState to;
  uint64_t timestamp;  // ms since epoch

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Records a single state transition in the arbitration engine.
 */
struct RequestTransition {
  RequestState from;
  RequestState to;
  uint64_t timestamp;  // ms since epoch

  void toJson(const JsonChunkVisitor& visitor) const;
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
  StaticSequence<detail::SequenceLimits::default_capacity> master;
  StaticSequence<detail::SequenceLimits::default_capacity> slave;
  uint64_t timestamp;  // ms since epoch

  void toJson(const JsonChunkVisitor& visitor) const;

  // Custom stringifier for human-readable logs
  std::string toString() const;

  void setMaster(const uint8_t* data, size_t len) { master.assign(data, len); }

  void setSlave(const uint8_t* data, size_t len) { slave.assign(data, len); }
};

static_assert(std::is_trivially_copyable_v<ErrorEntry>,
              "ErrorEntry must be trivially copyable for zero-allocation "
              "logging.");

}  // namespace ebus
