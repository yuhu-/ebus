/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/status.hpp>
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

namespace ebus::detail {

/**
 * Background worker that processes raw bytes from the Bus queue and feeds
 * them into the Request and Handler state machines. It also manages a
 * registry of byte listeners.
 */
class BusHandler {
 public:
  using ByteListener = std::function<void(const BusEventInfo& info)>;

  BusHandler(Request* request, Handler* handler,
             platform::Queue<BusEvent>* queue,
             size_t max_listeners = BusLimits::max_listeners)
      : request_(request), handler_(handler), queue_(queue), running_(false) {
    listeners_cache_.reserve(max_listeners);
  }

  ~BusHandler() { stop(); }

  void start() {
    if (running_) return;
    running_ = true;
    worker_ = std::make_unique<platform::ServiceThread>(
        "ebus_bus_handler", [this] { this->run(); },
        OrchestrationLimits::bus_handler_stack_size,
        OrchestrationLimits::bus_handler_priority);
    worker_->start();
  }

  void stop() {
    running_ = false;
    if (worker_) worker_->join();
  }

  void setWatchdogTimeout(uint32_t timeout_ms) {
    watchdog_timeout_ms_ = std::chrono::milliseconds(timeout_ms);
  }

  uint32_t addByteListener(ByteListener listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_listener_id_++;
    listeners_.push_back({id, std::move(listener)});
    listeners_version_++;
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
    listeners_version_++;
  }

  size_t queueSize() { return queue_ ? queue_->size() : 0; }

  size_t queueCapacity() const { return BusLimits::queue_size; }

  platform::ServiceThread::Status getThreadStatus() const {
    if (worker_) {
      return worker_->status();
    }
    return platform::ServiceThread::Status{"ebus_bus_handler", -1, -1};
  }

  BusHandlerStatus getStatus() {
    auto s = getThreadStatus();
    return {{s.name, s.task_stack_bytes, s.task_stack_free_bytes},
            queueSize(),
            queueCapacity(),
            max_queue_size_};
  }

 private:
  Request* request_;
  Handler* handler_;
  platform::Queue<BusEvent>* queue_;
  std::atomic<bool> running_;
  std::chrono::milliseconds watchdog_timeout_ms_{
      ebus::RuntimeConfig{}.bus.watchdog_timeout_ms};

  size_t max_queue_size_ = 0;
  std::unique_ptr<platform::ServiceThread> worker_;

  uint32_t next_listener_id_ = 0;
  mutable std::mutex mutex_;
  uint32_t listeners_version_ = 0;
  uint32_t last_cache_version_ = 0xffffffff;
  std::vector<std::pair<uint32_t, ByteListener>> listeners_;
  std::vector<ByteListener> listeners_cache_;

  void run() {
    BusEvent bus_event;
    while (running_) {
      if (queue_->pop(bus_event, watchdog_timeout_ms_)) {
        size_t q_size = queueSize() + 1;  // Current popped item
        if (q_size > max_queue_size_) max_queue_size_ = q_size;

        BusEventInfo info{bus_event.byte,
                          HandlerState::passive_receive_master,
                          RequestState::observe,
                          RequestResult::observe_data,
                          0,
                          bus_event.timestamp};

        if (request_) {
          if (bus_event.bus_request) request_->busRequestCompleted();
          if (bus_event.start_bit) request_->startBit();
          info.request_state = request_->getState();
          info.result = request_->run(bus_event.byte);
          info.lock_counter = request_->getLockCounter();
        }
        if (handler_) {
          handler_->run(info);
          info.handler_state = handler_->getState();
        }

        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (listeners_version_ != last_cache_version_) {
            listeners_cache_.clear();
            for (const auto& item : listeners_)
              listeners_cache_.push_back(item.second);
            last_cache_version_ = listeners_version_;
          }
        }
        // Execute listeners outside the lock to prevent deadlocks
        if (!listeners_cache_.empty()) {
          for (const auto& listener : listeners_cache_) {
            listener(info);
          }
        }
      }
    }
  }
};

}  // namespace ebus::detail
