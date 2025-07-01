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

#include "BusFreeRtos.hpp"

#include "../Handler.hpp"

void ebus::BusFreeRtos::setHandler(Handler* handler) {
  this->handler = handler;
}

void ebus::BusFreeRtos::setRequestWindow(const uint16_t& delay) {
  requestWindow = delay;
}
void ebus::BusFreeRtos::begin(const uint32_t& baud, const int8_t& rx_pin,
                              const int8_t& tx_pin) {
  serial.begin(baud, SERIAL_8N1, rx_pin, tx_pin);

  // Setup timer for precise bus request
  uint32_t timer_clk = APB_CLK_FREQ;       // Usually 80 MHz
  uint16_t divider = timer_clk / 1000000;  // For 1us tick
  requestBusTimer = timerBegin(0, divider, true);
  timerAttachInterrupt(requestBusTimer,
                       reinterpret_cast<void (*)()>(onRequestBusTimerStatic),
                       true);
  timerAlarmDisable(requestBusTimer);

  // Attach the ISR to the UART receive event
  serial.onReceive([this]() { this->onUartRx(); });
}

void ebus::BusFreeRtos::writeByte(const uint8_t& byte) { serial.write(byte); }

void IRAM_ATTR ebus::BusFreeRtos::onRequestBusTimerStatic(void* arg) {
  static_cast<BusFreeRtos*>(arg)->onRequestBusTimer();
}

void IRAM_ATTR ebus::BusFreeRtos::onRequestBusTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (requestBusPending && !serial.available()) {
    serial.write(handler->getAddress());
    requestBusPending = false;
    requestBusDone = true;
  }
  timerAlarmDisable(requestBusTimer);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR ebus::BusFreeRtos::onUartRx() {
  while (serial.available()) {
    uint32_t microsStartBit = micros() - 4167;  // 4167us = 2400 baud, 10 bit
    uint8_t byte = serial.read();

    // Handle bus request done flag
    if (requestBusDone) {
      requestBusDone = false;
      if (handler) handler->busRequested();
    }

    // Push byte to queue (should be lock-free or ISR-safe)
    byteQueue.push(byte);

    // Handle bus request logic only if needed
    if (byte == ebus::sym_syn) {
      if (handler && handler->busRequest()) {
        portENTER_CRITICAL_ISR(&timerMux);
        int32_t delay = requestWindow - (micros() - microsStartBit);
        if (delay > 0) {
          requestBusPending = true;
          timerAlarmWrite(requestBusTimer, delay, false);
          timerAlarmEnable(requestBusTimer);
        } else {
          // Only write if TX buffer is empty
          if (!serial.available()) {
            serial.write(handler->getAddress());
            requestBusPending = false;
            requestBusDone = true;
          }
        }
        portEXIT_CRITICAL_ISR(&timerMux);
      }
    }
  }
}
