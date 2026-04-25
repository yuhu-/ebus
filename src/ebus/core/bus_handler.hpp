/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "core/bus_events.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

namespace ebus {

/**
 * Background worker that processes raw bytes from the Bus queue and feeds
 * them into the Request and Handler state machines. It also manages a
 * registry of byte listeners.
 */
class BusHandler {
 public:
  using ByteListener = std::function<void(const BusEventContext& ctx)>;

  BusHandler(Request* request, Handler* handler, Queue<BusEvent>* queue,
             size_t max_listeners = 16)
      : request_(request), handler_(handler), queue_(queue), running_(false) {
    listeners_cache_.reserve(max_listeners);
  }

  ~BusHandler() { stop(); }

  void start() {
    if (running_) return;
    running_ = true;
    worker_ = std::make_unique<ServiceThread>(
        "ebusBusQueueRunner", [this] { this->run(); }, 4096, 1);
    worker_->start();
  }

  void stop() {
    running_ = false;
    if (worker_) worker_->join();
  }

  void setWatchdogTimeout(std::chrono::milliseconds timeout) {
    watchdog_timeout_ms_ = timeout;
  }

  uint32_t addByteListener(ByteListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_listener_id_++;
    listeners_.push_back({id, std::move(listener)});
    return id;
  }

  void removeByteListener(uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
                       [id](const std::pair<uint32_t, ByteListener>& p) {
                         return p.first == id;
                       }),
        listeners_.end());
  }

 private:
  Request* request_;
  Handler* handler_;
  Queue<BusEvent>* queue_;
  std::atomic<bool> running_;
  std::chrono::milliseconds watchdog_timeout_ms_{
      defaults::Network::watchdog_timeout_ms};

  std::unique_ptr<ServiceThread> worker_;

  uint32_t next_listener_id_ = 0;
  mutable std::mutex mutex_;
  std::vector<std::pair<uint32_t, ByteListener>> listeners_;
  std::vector<ByteListener> listeners_cache_;

  void run() {
    BusEvent bus_event;
    while (running_) {
      if (queue_->pop(bus_event, watchdog_timeout_ms_)) {
        BusEventContext ctx{bus_event.byte, RequestState::observe,
                            RequestResult::observe_data, 0,
                            bus_event.timestamp};

        if (request_) {
          if (bus_event.bus_request) request_->busRequestCompleted();
          if (bus_event.start_bit) request_->startBit();
          ctx.state = request_->getState();
          ctx.result = request_->run(bus_event.byte);
          ctx.lock_counter = request_->getLockCounter();
        }
        if (handler_) handler_->run(ctx);

        {
          // Only lock and copy if listeners actually exist.
          // Note: read size() without lock is acceptable for this check.
          if (!listeners_.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            listeners_cache_.clear();
            for (const auto& item : listeners_)
              listeners_cache_.push_back(item.second);

            for (const auto& listener : listeners_cache_) listener(ctx);
          }
        }
      }
    }
  }
};

}  // namespace ebus
