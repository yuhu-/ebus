/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ebus/detail/protocol_limits.hpp"
#include "ebus/types.hpp"

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

namespace ebus {

/**
 * Forward declaration to avoid pulling in the heavy sequence.hpp
 */
template <std::size_t kInlineCapacity>
class SequenceImpl;
using Sequence = SequenceImpl<detail::SequenceLimits::default_capacity>;

struct TelegramInfo {
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  uint32_t retry_count = 0;
  MessageType message_type;
  TelegramType telegram_type;
  HandlerState handler_state;
  RequestState request_state;
  ByteView master_view;
  ByteView slave_view;

  void toJson(const JsonChunkVisitor& visitor) const;
};

struct ErrorInfo {
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  LogLevel level = LogLevel::error;
  ProtocolError protocol_error = ProtocolError::none;
  RequestResult result;
  SequenceState sequence_state;
  HandlerState handler_state;
  RequestState request_state;
  ByteView master_view;
  ByteView slave_view;

  void toJson(const JsonChunkVisitor& visitor) const;
};

struct ReactiveInfo {
  uint32_t session_id = 0;
  ByteView master_view;
  Sequence& slave_response;

  void toJson(const JsonChunkVisitor& visitor) const;
};

struct ResultInfo {
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  bool success;
  RequestResult result;
  SequenceState sequence_state;
  ByteView master_view;
  ByteView slave_view;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 * Included in public callbacks for protocol tracing and diagnostics.
 */
struct BusEventInfo {
  uint8_t byte = 0;
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;
  RequestResult result = RequestResult::observe_data;
  uint8_t lock_counter = 0;
  Clock::time_point timestamp = {};

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Internal carrier for decoupled public callbacks.
 * Contains owning copies of byte sequences.
 */
struct ProtocolEvent {
  enum class Type : uint8_t { telegram, error } type;

  // Shared metadata (Ordered to minimize padding)
  uint32_t session_id;
  uint32_t poll_id;
  HandlerState handler_state;  // uint8_t
  RequestState request_state;  // uint8_t

  union {
    struct {
      uint32_t retry_count;
      MessageType message_type;    // uint8_t
      TelegramType telegram_type;  // uint8_t
    } tel;
    struct {
      ProtocolError protocol_error;  // uint8_t
      RequestResult result;          // uint8_t
      SequenceState sequence_state;  // uint8_t
      LogLevel level;                // uint8_t
    } err;
  } data;

  StaticSequence<detail::SequenceLimits::default_capacity> master;
  StaticSequence<detail::SequenceLimits::default_capacity> slave;
};

static_assert(
    std::is_trivially_copyable_v<ProtocolEvent>,
    "ProtocolEvent must be trivially copyable for FreeRTOS queue safety.");
static_assert(
    sizeof(ProtocolEvent) <= 192,
    "ProtocolEvent exceeds the memory threshold for constrained targets. "
    "Verify enum packing and buffer sizes.");

/**
 * Callback signatures
 */
using TelegramCallback = std::function<void(const TelegramInfo& info)>;

using ErrorCallback = std::function<void(const ErrorInfo& info)>;

using ReactiveMasterSlaveCallback =
    std::function<void(const ReactiveInfo& info)>;

using ResultCallback = std::function<void(const ResultInfo& info)>;

using TraceCallback = std::function<void(const BusEventInfo& info)>;

}  // namespace ebus
