/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ebus/byte_view.hpp"
#include "ebus/detail/protocol_limits.hpp"
#include "ebus/types.hpp"

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

  /**
   * @brief Serializes TelegramInfo to a JSON object string.
   */
  std::string toJson() const;
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
  float utilization = 0.0f;

  /**
   * @brief Serializes ErrorInfo to a JSON object string.
   */
  std::string toJson() const;
};

struct ReactiveInfo {
  uint32_t session_id = 0;
  ByteView master_view;
  Sequence& slave_response;

  /**
   * @brief Serializes ReactiveInfo to a JSON object string.
   */
  std::string toJson() const;
};

struct ResultInfo {
  uint32_t session_id = 0;
  uint32_t poll_id = 0;
  bool success;
  RequestResult result;
  SequenceState sequence_state;
  ByteView master_view;
  ByteView slave_view;

  /**
   * @brief Serializes ResultInfo to a JSON object string.
   */
  std::string toJson() const;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 * Included in public callbacks for protocol tracing and diagnostics.
 */
struct BusEventContext {
  uint8_t byte = 0;
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;
  RequestResult result = RequestResult::observe_data;
  uint8_t lock_counter = 0;
  Clock::time_point timestamp = {};

  /**
   * Serializes BusEventContext to a JSON object string.
   */
  std::string toJson() const;
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
  HandlerState handler_state;
  RequestState request_state;

  union {
    struct {
      uint32_t retry_count;
      MessageType message_type;
      TelegramType telegram_type;
    } tel;
    struct {
      float utilization;
      ProtocolError protocol_error;
      RequestResult result;
      SequenceState sequence_state;
      LogLevel level;
    } err;
  } data;

  uint8_t master[detail::SequenceLimits::default_capacity];
  uint8_t slave[detail::SequenceLimits::default_capacity];
  uint8_t master_len;
  uint8_t slave_len;
};

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

using TraceCallback = std::function<void(const BusEventContext& ctx)>;

}  // namespace ebus
