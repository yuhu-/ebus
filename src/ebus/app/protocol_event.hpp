/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

#include "ebus/callbacks.hpp"
#include "ebus/detail/protocol_limits.hpp"

namespace ebus::detail {

/**
 * Internal carrier for protocol results and decoupled public callbacks.
 * Transported between Scheduler, Reactor, and Controller::Impl.
 * Not exposed in the public API.
 */
struct ProtocolEvent {
  enum class Type : uint8_t { won, lost, telegram, error } type;

  // Common metadata
  LogLevel level;
  uint64_t timestamp = 0;  // ms since epoch

  uint32_t session_id;
  uint16_t poll_id;
  uint8_t attempts;

  HandlerState handler_state;
  RequestState request_state;

  // Telegram-specific fields
  MessageType message_type = MessageType::undefined;
  TelegramType telegram_type = TelegramType::undefined;

  // Error-specific fields
  ProtocolError protocol_error = ProtocolError::none;
  RequestResult result = RequestResult::observe_data;
  SequenceState sequence_state = SequenceState::seq_empty;

  // Optimization for ESP32-C3: Reduced buffer size for internal event
  // passing. Logical eBUS telegrams are max 21 bytes (master) / 17 bytes
  // (slave).
  StaticSequence<detail::SequenceLimits::model_capacity> master;
  StaticSequence<detail::SequenceLimits::model_capacity> slave;
};

static_assert(std::is_trivially_copyable_v<ProtocolEvent>,
              "ProtocolEvent must be trivially copyable for Reactor Queue.");
static_assert(
    sizeof(ProtocolEvent) <= 88,
    "ProtocolEvent exceeds the memory threshold for constrained targets. "
    "Verify enum packing and buffer sizes.");

}  // namespace ebus::detail
