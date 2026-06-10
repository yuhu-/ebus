/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>

#include <app/client.hpp>
#include <app/client_manager.hpp>
#include <app/scheduler.hpp>
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
#include "platform/mutex.hpp"
#include "platform/service_thread.hpp"
#include "platform/socket.hpp"
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
  const auto start = ebus::Clock::now();
  while (ebus::Clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
    if (pred()) {
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }
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

  ClientInfo getClientInfo() const override {
    // Mock implementation for testing purposes
    return ClientInfo{fd_, "mock", isConnected(), write_capable_,
                      outbound_.size()};
  }

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

/**
 * Pumper to drive the state machines of passive components during tests.
 * Replaces background threads with explicit orchestration in the Reactor
 * model.
 */
class TestReactor {
 public:
  TestReactor(platform::Bus& bus, BusHandler& busHandler,
              ClientManager* manager = nullptr, Scheduler* scheduler = nullptr)
      : bus_(bus),
        busHandler_(busHandler),
        manager_(manager),
        scheduler_(scheduler) {
    bus_.addBusEventListener(platform::Delegate<void(const BusEvent&)>::bind<
                             TestReactor, &TestReactor::onBusEvent>(this));
  }

  void onBusEvent(const BusEvent& ev) {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    events_.push(ev);
  }

  /**
   * @brief Performs a single cycle of the orchestration loop.
   * Pulsates all managers and drains the physical bus event queue.
   */
  void pump() {
    if (scheduler_) scheduler_->tick();
    if (manager_) manager_->tick();

    platform::UniqueLock<platform::Mutex> lock(mutex_);
    while (!events_.empty()) {
      BusEvent ev = events_.front();
      events_.pop();
      lock.unlock();
      busHandler_.processEvent(ev);
      lock.lock();
    }
  }

  /**
   * @brief Blocks until the predicate is true, pumping the reactor in a loop.
   */
  template <typename Predicate>
  bool waitFor(Predicate&& pred,
               std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    // Always pump at least once to ensure progress even if pred is already true
    pump();
    auto start = Clock::now();
    while (!pred() && (Clock::now() - start < timeout)) {
      pump();
      platform::sleepMilli(1);
    }
    return pred();
  }

  /**
   * @brief Reads from a socket while pumping the reactor to avoid deadlocks.
   * Replaces blocking readExact for network bridge tests.
   */
  bool readFromSocket(
      int fd, uint8_t* buf, size_t len,
      std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    size_t received = 0;
    return waitFor(
        [&]() {
          ssize_t n = platform::recv(fd, buf + received, len - received,
                                     platform::Flags::dont_wait);
          if (n > 0) received += static_cast<size_t>(n);
          return received == len;
        },
        timeout);
  }

  /**
   * auto cmds = reactor.collect(dm, &DeviceManager::vendorScanCommands);
   */
  template <typename T, typename Func>
  auto collect(T& obj, Func member_func) {
    std::vector<ebus::Sequence> captured;
    (obj.*
     member_func)([&](const ebus::Sequence& cmd) { captured.push_back(cmd); });
    return captured;
  }

 private:
  platform::Bus& bus_;
  BusHandler& busHandler_;
  ClientManager* manager_;
  Scheduler* scheduler_;
  platform::Mutex mutex_;
  std::queue<BusEvent> events_;
};

}  // namespace ebus::detail
