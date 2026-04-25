/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string_view>

#include "ebus/sequence.hpp"
#include "ebus/types.hpp"

namespace ebus {

struct TelegramInfo {
  uint32_t session_id = 0;
  uint32_t retry_count = 0;
  MessageType message_type;
  TelegramType telegram_type;
  HandlerState handler_state;
  RequestState request_state;
  ByteView master;
  ByteView slave;
};

struct ErrorInfo {
  uint32_t session_id = 0;
  LogLevel level;
  std::string_view message;
  RequestResult result;
  HandlerState handler_state;
  RequestState request_state;
  ByteView master;
  ByteView slave;
  double utilization = 0.0;
};

struct ReactiveInfo {
  uint32_t session_id = 0;
  ByteView master;
  Sequence& response;
};

struct ResultInfo {
  uint32_t session_id = 0;
  bool success;
  ByteView master;
  ByteView slave;
};

/**
 * Serializes ErrorInfo to a JSON object string.
 */
std::string toJson(const ErrorInfo& info);

/**
 * Callback signatures
 */
using TelegramCallback = std::function<void(const TelegramInfo& info)>;

using ErrorCallback = std::function<void(const ErrorInfo& info)>;

using ReactiveMasterSlaveCallback =
    std::function<void(const ReactiveInfo& info)>;

using ResultCallback = std::function<void(const ResultInfo& info)>;

}  // namespace ebus
