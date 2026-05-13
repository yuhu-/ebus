/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>

#include <app/client.hpp>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/bus.hpp"
#include "platform/service_thread.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

/**
 * Robust read helper to handle partial TCP/Socket reads.
 */
inline bool readExact(int fd, uint8_t* buffer, size_t length) {
  size_t total = 0;
  while (total < length) {
    ssize_t n = read(fd, buffer + total, length - total);
    if (n <= 0) return false;
    total += n;
  }
  return true;
}

/**
 * Helper to wait for a condition to become true without hardcoded sleeps.
 */
template <typename Predicate>
inline bool waitCondition(Predicate&& pred, int timeout_ms = 1000) {
  auto start = ebus::Clock::now();
  while (ebus::Clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
    if (pred()) return true;
    platform::sleepMilli(5);
  }
  return pred();
}

/**
 * In-memory client for testing ClientManager without real sockets.
 */
class MockClient : public detail::AbstractClient {
 public:
  explicit MockClient(Request* req, bool write_capable = true,
                      size_t max_buffer = 1024)
      : AbstractClient(999, req, write_capable, max_buffer) {}

  ~MockClient() override { fd_ = -1; }

  void onSessionStart(uint32_t session_id) override { (void)session_id; }

  bool wantsToSend() override { return !inbound_.empty(); }

  bool recvFromClient(uint8_t& out) override {
    if (inbound_.empty()) return false;
    out = inbound_.front();
    last_sent_byte_ = out;
    inbound_.pop();
    return true;
  }

  void sendToClient(ByteView data) override {
    if (outbound_.size() + data.size() > max_buffer_size_) {
      this->stop();  // Simulate socket closing on overflow
      return;
    }
    outbound_.insert(outbound_.end(), data.begin(), data.end());
  }

  BridgeAction onBusByte(const BusEventInfo& info) override {
    if (!this->isConnected()) return BridgeAction::stop_session;

    bool proceed = false;
    switch (info.result) {
      case RequestResult::first_won:
      case RequestResult::second_won:
        proceed = true;
        break;
      case RequestResult::observe_data:
        if (info.byte == last_sent_byte_) {
          proceed = true;
        }
        break;
      default:
        break;
    }

    sendToClient(ByteView(&info.byte, 1));
    return proceed ? BridgeAction::bypass_wait : BridgeAction::keep_active;
  }

  void pushInput(uint8_t b) { inbound_.push(b); }
  const std::vector<uint8_t>& getOutput() const { return outbound_; }

 private:
  std::queue<uint8_t> inbound_;
  std::vector<uint8_t> outbound_;
};

}  // namespace ebus::detail
