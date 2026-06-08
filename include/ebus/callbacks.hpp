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

/**
 * Unified carrier for protocol results (Success or Error).
 * Delivered to the user via the decoupled ProtocolCallback.
 */
struct ProtocolInfo {
  ProtocolInfo() = default;

  bool is_error = false;
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;
  uint32_t retry_count = 0;
  ByteView master_view;
  ByteView slave_view;

  // Telegram-specific fields
  MessageType message_type = MessageType::undefined;
  TelegramType telegram_type = TelegramType::undefined;

  // Error-specific fields
  LogLevel level = LogLevel::none;
  ProtocolError protocol_error = ProtocolError::none;
  RequestResult result = RequestResult::observe_data;
  SequenceState sequence_state = SequenceState::seq_empty;

  void toJson(detail::JsonWriter& writer) const;
};

struct ReactiveInfo {
  ReactiveInfo(uint32_t s_id, ByteView master, Sequence& slave)
      : session_id(s_id), master_view(master), slave_response(slave) {}

  uint32_t session_id = 0;
  ByteView master_view;
  Sequence& slave_response;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 * Included in public callbacks for protocol tracing and diagnostics.
 */
struct BusEventInfo {
  BusEventInfo() = default;
  BusEventInfo(uint8_t b, HandlerState hs, RequestState rs, RequestResult res,
               uint8_t lc, Clock::time_point ts)
      : byte(b),
        handler_state(hs),
        request_state(rs),
        result(res),
        lock_counter(lc),
        timestamp(ts) {}

  uint8_t byte = 0;
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;
  RequestResult result = RequestResult::observe_data;
  uint8_t lock_counter = 0;
  Clock::time_point timestamp = {};

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Callback signatures
 */
using ProtocolCallback = std::function<void(const ProtocolInfo& info)>;

using ReactiveMasterSlaveCallback =
    std::function<void(const ReactiveInfo& info)>;

using TraceCallback = std::function<void(const BusEventInfo& info)>;

}  // namespace ebus
