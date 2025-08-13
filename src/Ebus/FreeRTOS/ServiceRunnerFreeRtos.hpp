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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vector>

#include "../Handler.hpp"
#include "../Queue.hpp"
#include "../Request.hpp"

namespace ebus {

extern void processBusIsrEvents();

class ServiceRunnerFreeRtos {
 public:
  // Define a listener type for byte events
  using ByteListener = std::function<void(const uint8_t& byte)>;

  ServiceRunnerFreeRtos(Request& request, Handler& handler,
                        Queue<uint8_t>& queue)
      : request(request), handler(handler), queue(queue), taskHandle(nullptr) {}

  void start() {
    xTaskCreatePinnedToCore(&ServiceRunnerFreeRtos::taskFunc,
                            "ebusServiceRunner", 4096, this, 1, &taskHandle,
                            tskNO_AFFINITY);
  }

  void stop() {
    if (taskHandle) {
      vTaskDelete(taskHandle);
      taskHandle = nullptr;
    }
  }

  // Register a listener for incoming bytes
  void addByteListener(ByteListener listener) { listeners.push_back(listener); }

 private:
  Request& request;
  Handler& handler;
  Queue<uint8_t>& queue;
  TaskHandle_t taskHandle;
  std::vector<ByteListener> listeners;

  static void taskFunc(void* arg) {
    ServiceRunnerFreeRtos* self = static_cast<ServiceRunnerFreeRtos*>(arg);
    uint8_t byte;
    for (;;) {
      // Blocking
      // Comment out the following line if you want non-blocking behavior
      if (self->queue.pop(byte)) {
        processBusIsrEvents();
        self->request.run(byte);
        self->handler.run(byte);
        for (const ByteListener& listener : self->listeners) listener(byte);
      }
      // Non-blocking
      // Uncomment the following lines if you want non-blocking behavior
      // while (self->queue.try_pop(byte)) {  // non-blocking
      //   processBusIsrEvents();
      //   self->request.run(byte);
      //   self->handler.run(byte);
      //   for (const ByteListener& listener : self->listeners) listener(byte);
      // }
      // vTaskDelay(pdMS_TO_TICKS(1));  // short delay to yield CPU
    }
  }
};

}  // namespace ebus
