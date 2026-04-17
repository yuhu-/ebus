/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>
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

// --- Protocol Enums ---
enum class TelegramType { undefined, broadcast, master_master, master_slave };
enum class MessageType { undefined, active, passive, reactive };

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

// --- Public Callback Signatures ---

/**
 * Callback for every valid telegram captured on the bus.
 */
using TelegramCallback =
    std::function<void(MessageType message_type, TelegramType telegram_type,
                       ByteView master_view, ByteView slave_view)>;

/**
 * Callback for protocol or hardware errors.
 */
using ErrorCallback = std::function<void(
    std::string_view error_message, ByteView master_view, ByteView slave_view)>;

/**
 * Result of a message enqueued via the Scheduler.
 */
using ResultCallback = std::function<void(bool success, ByteView master_view,
                                          ByteView slave_view)>;

}  // namespace ebus