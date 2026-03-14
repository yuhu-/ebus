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

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vector>

#include "../Handler.hpp"
#include "../Queue.hpp"
#include "../Request.hpp"

namespace ebus {

class BusHandlerFreeRtos {
 public:
  // Define a listener type for byte events
  using ByteListener = std::function<void(const uint8_t& byte)>;

  BusHandlerFreeRtos(Request* request, Handler* handler, Queue<BusEvent>* queue)
      : request_(request),
        handler_(handler),
        queue_(queue),
        taskHandle_(nullptr) {}

  void start() {
    xTaskCreatePinnedToCore(&BusHandlerFreeRtos::taskFunc, "ebusBusQueueRunner",
                            4096, this, 1, &taskHandle_, tskNO_AFFINITY);
  }

  void stop() {
    if (taskHandle_) {
      vTaskDelete(taskHandle_);
      taskHandle_ = nullptr;
    }
  }

  // Register a listener for incoming bytes
  void addByteListener(ByteListener listener) {
    listeners_.push_back(listener);
  }

 private:
  Request* request_;
  Handler* handler_;
  Queue<BusEvent>* queue_;
  TaskHandle_t taskHandle_;
  std::vector<ByteListener> listeners_;

  static void taskFunc(void* arg) {
    BusHandlerFreeRtos* self = static_cast<BusHandlerFreeRtos*>(arg);
    BusEvent event;
    for (;;) {
      if (self->queue_->pop(event)) {
        if (event.busRequest) self->request_->busRequestCompleted();
        if (event.startBit) self->request_->startBit();
        self->request_->run(event.byte);
        self->handler_->run(event.byte);
        for (const ByteListener& listener : self->listeners_)
          listener(event.byte);
      }
    }
  }
};

}  // namespace ebus

#endif