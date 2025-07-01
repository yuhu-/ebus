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

#pragma once

#include <HardwareSerial.h>
#include <driver/timer.h>

#include <atomic>

#include "../Queue.hpp"

// Forward declaration to avoid circular dependency
namespace ebus {
class Handler;
}

namespace ebus {

class BusFreeRtos {
 public:
  BusFreeRtos(HardwareSerial& serial, Queue<uint8_t>& byteQueue)
      : serial(serial), byteQueue(byteQueue) {}

  void setHandler(Handler* handler);

  void setRequestWindow(const uint16_t& delay);

  void begin(const uint32_t& baud, const int8_t& rx_pin, const int8_t& tx_pin);

  void writeByte(const uint8_t& byte);

 private:
  HardwareSerial& serial;
  Queue<uint8_t>& byteQueue;
  Handler* handler = nullptr;

  hw_timer_t* requestBusTimer = nullptr;
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

  volatile uint16_t requestWindow = 4300;  // default 4300us
  std::atomic<bool> requestBusPending{false};
  std::atomic<bool> requestBusDone{false};

  static void IRAM_ATTR onRequestBusTimerStatic(void* arg);
  void IRAM_ATTR onRequestBusTimer();
  void IRAM_ATTR onUartRx();
};

}  // namespace ebus
