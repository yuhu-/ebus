/*
 * Copyright (C) 2025-2026 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#pragma once

#if defined(POSIX)
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "../Handler.hpp"
#include "../Queue.hpp"
#include "../Request.hpp"

namespace ebus {

class BusHandlerPosix {
 public:
  // Define a listener type for byte events
  using ByteListener = std::function<void(const uint8_t& byte)>;

  BusHandlerPosix(Request* request, Handler* handler, Queue<BusEvent>* queue)
      : request_(request), handler_(handler), queue_(queue), running_(false) {}

  void start() {
    running_ = true;
    thread_ = std::thread([this]() { this->run(); });
  }

  void stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
  }

  void enableTesting() { testing_ = true; }

  // Register a listener for incoming bytes
  void addByteListener(ByteListener listener) {
    listeners_.push_back(listener);
  }

 private:
  Request* request_;
  Handler* handler_;
  Queue<BusEvent>* queue_;
  std::atomic<bool> running_;
  std::thread thread_;
  bool testing_ = false;
  std::vector<ByteListener> listeners_;

  void run() {
    BusEvent event;
    while (running_) {
      if (queue_->pop(event)) {
        if (testing_) {
          for (const ByteListener& listener : listeners_) listener(event.byte);
          if (event.busRequest) request_->busRequestCompleted();
          if (event.startBit) request_->startBit();
          request_->run(event.byte);
          handler_->run(event.byte);
        } else {
          if (event.busRequest) request_->busRequestCompleted();
          if (event.startBit) request_->startBit();
          request_->run(event.byte);
          handler_->run(event.byte);
          for (const ByteListener& listener : listeners_) listener(event.byte);
        }
      }
    }
  }
};

}  // namespace ebus

#endif