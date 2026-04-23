/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ebus {

// --- Protocol Symbols ---
constexpr uint8_t sym_zero = 0x00;     // zero byte
constexpr uint8_t sym_syn = 0xaa;      // synchronization byte
constexpr uint8_t sym_ext = 0xa9;      // extend byte
constexpr uint8_t sym_syn_ext = 0x01;  // extended synchronization byte
constexpr uint8_t sym_ext_ext = 0x00;  // extended extend byte
constexpr uint8_t sym_ack = 0x00;      // positive acknowledge
constexpr uint8_t sym_nak = 0xff;      // negative acknowledge
constexpr uint8_t sym_broad = 0xfe;    // broadcast destination address

// --- Protocol Limits ---
constexpr uint8_t max_bytes = 0x10;  // 16 maximum data bytes (Spec 5.6)

// --- FSM Limits ---
constexpr size_t NUM_HANDLER_STATES = 15;
constexpr size_t NUM_REQUEST_STATES = 4;

// --- Protocol Enums ---
enum class TelegramType { undefined, broadcast, master_master, master_slave };
enum class MessageType { undefined, active, passive, reactive };

// --- Protocol States ---
enum class SequenceState {
  seq_empty,           // sequence is empty
  seq_ok,              // sequence is ok
  err_seq_too_short,   // sequence is too short
  err_seq_too_long,    // sequence is too long
  err_source_address,  // source address is invalid
  err_target_address,  // target address is invalid
  err_data_byte,       // data byte is invalid
  err_crc_invalid,     // CRC byte is invalid
  err_ack_invalid,     // acknowledge byte is invalid
  err_ack_missing,     // acknowledge byte is missing
  err_ack_negative     // acknowledge byte is negative
};

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

enum class RequestState { observe, first, retry, second };

constexpr const char* toString(RequestState state) {
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

constexpr const char* toString(RequestResult result) {
  switch (result) {
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
      return "unknown_result";
  }
}

// --- Protocol Structs ---

/**
 * Represents a single event on the bus, including the byte value, whether it
 * was associated with a bus request or start bit, and the timestamp of when it
 * was captured.
 */
struct BusEvent {
  uint8_t byte;
  bool bus_request{false};
  bool start_bit{false};
  std::chrono::steady_clock::time_point timestamp;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 */
struct BusEventContext {
  uint8_t byte;
  RequestState state;
  RequestResult result;
  uint8_t lock_counter;
  std::chrono::steady_clock::time_point timestamp;
};

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

// --- Public Callback Signatures ---

/**
 * Callback for every valid telegram captured on the bus.
 */
using TelegramCallback =
    std::function<void(MessageType message_type, TelegramType telegram_type,
                       ByteView master_view, ByteView slave_view)>;

/**
 * Callback for protocol or hardware errors.
 *
 * IMPORTANT: The `error_message` parameter is a `std::string_view`.
 * If the underlying string data is not static (e.g., a string literal)
 * or does not outlive the processing of the error event, it can lead
 * to dangling pointers. Ensure the lifetime of the string data.
 */
using ErrorCallback = std::function<void(
    std::string_view error_message, ByteView master_view, ByteView slave_view)>;

}  // namespace ebus