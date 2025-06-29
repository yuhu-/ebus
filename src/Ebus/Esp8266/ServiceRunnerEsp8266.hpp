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

#include "../Handler.hpp"
#include "../Queue.hpp"

namespace ebus {

class ServiceRunnerEsp8266 {
 public:
  ServiceRunnerEsp8266(Handler& handler, Queue& queue)
      : handler(handler), queue(queue) {}

  // Call this frequently in the main loop!
  void poll() {
    uint8_t byte;
    while (queue.pop(byte)) {
      handler.run(byte);
    }
  }

 private:
  Handler& handler;
  Queue& queue;
};

}  // namespace ebus
