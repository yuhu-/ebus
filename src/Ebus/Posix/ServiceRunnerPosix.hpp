/*
 * Copyright (C) 2025 Roland Jax
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

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "../Handler.hpp"
#include "../Queue.hpp"
#include "../Request.hpp"

namespace ebus {

class ServiceRunnerPosix {
 public:
  // Define a listener type for byte events
  using ByteListener = std::function<void(const uint8_t&)>;

  ServiceRunnerPosix(Request& request, Handler& handler, Queue<uint8_t>& queue)
      : request(request), handler(handler), queue(queue), running(false) {}

  void start() {
    running = true;
    thread = std::thread([this]() { this->run(); });
  }

  void stop() {
    running = false;
    if (thread.joinable()) thread.join();
  }

  void enableTesting() { testing = true; }

  // Register a listener for incoming bytes
  void addByteListener(ByteListener listener) { listeners.push_back(listener); }

 private:
  Request& request;
  Handler& handler;
  Queue<uint8_t>& queue;
  std::atomic<bool> running;
  std::thread thread;
  bool testing = false;
  std::vector<ByteListener> listeners;

  void run() {
    while (running) {
      uint8_t byte;
      if (queue.pop(byte)) {
        if (!testing) {
          request.run(byte);
          handler.run(byte);
        }
        for (const ByteListener& listener : listeners) listener(byte);
        if (testing) {
          request.run(byte);
          handler.run(byte);
        }
      }
    }
  }
};

}  // namespace ebus
