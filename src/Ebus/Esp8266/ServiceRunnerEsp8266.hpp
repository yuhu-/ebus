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

#include <vector>

#include "../Handler.hpp"
#include "../Queue.hpp"
#include "../Request.hpp"

namespace ebus {

class ServiceRunnerEsp8266 {
 public:
  // Define a listener type for byte events
  using ByteListener = std::function<void(const uint8_t& byte)>;

  ServiceRunnerEsp8266(Request& request, Handler& handler, Queue& queue)
      : request(request), handler(handler), queue(queue) {}

  // Call this frequently in the main loop!
  void poll() {
    uint8_t byte;
    while (queue.pop(byte)) {
      request.run(byte);
      handler.run(byte);
      for (const ByteListener& listener : listeners) listener(byte);
    }
  }

  // Register a listener for incoming bytes
  void addByteListener(ByteListener listener) { listeners.push_back(listener); }

 private:
  Request& request;
  Handler& handler;
  Queue& queue;
  std::vector<ByteListener> listeners;
};

}  // namespace ebus
