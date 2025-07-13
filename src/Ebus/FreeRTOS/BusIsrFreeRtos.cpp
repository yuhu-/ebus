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

#include "BusIsrFreeRtos.hpp"

hw_timer_t* requestBusTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// This value can be adjusted if the bus request is not working as expected.
volatile uint16_t requestOffset = 100;

volatile bool requestBusDone = false;

volatile uint32_t microsLastFallingEdge = 0;
volatile uint32_t microsLastStartBitAutoSyn = 0;

static HardwareSerial* hwSerial = nullptr;

ebus::Bus* bus = nullptr;
ebus::Queue<uint8_t>* byteQueue = nullptr;
ebus::Handler* ebus::handler = nullptr;
ebus::ServiceRunner* ebus::serviceRunner = nullptr;

// ISR: Fires when a byte is received
void IRAM_ATTR onUartRx() {
  if (!byteQueue || !ebus::handler) return;

  // Read all available bytes as quickly as possible
  while (hwSerial->available()) {
    // Handle bus request done flag
    portENTER_CRITICAL_ISR(&timerMux);
    if (requestBusDone) {
      requestBusDone = false;
      ebus::handler->busRequested();
    }
    portEXIT_CRITICAL_ISR(&timerMux);

    uint8_t byte = hwSerial->read();
    // Push byte to queue (should be lock-free or ISR-safe)
    byteQueue->push(byte);

    // Handle bus request logic only if needed
    if (byte == ebus::sym_syn && ebus::handler->busRequest()) {
      uint32_t microsSinceLastStartBit = micros() - microsLastStartBitAutoSyn;
      int64_t delay = 4300 - microsSinceLastStartBit - requestOffset;
      if (delay > 0) {
        timerAlarmWrite(requestBusTimer, delay, false);
        timerAlarmEnable(requestBusTimer);
        ebus::handler->busRequestDelay(delay);
      }
    }
  }
}

// ISR: Write request byte at the exact time
void IRAM_ATTR onRequestBusTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (!hwSerial->available()) {
    requestBusDone = true;
    hwSerial->write(ebus::handler->getAddress());
  }
  portEXIT_CRITICAL_ISR(&timerMux);
  timerAlarmDisable(requestBusTimer);
}

// ISR: Save the timestamp of the first bit (falling edge) from AutoSyn
void IRAM_ATTR onFallingEdge() {
  uint32_t now = micros();
  uint32_t duration = now - microsLastFallingEdge;
  if (duration > 35000)  // AutoSyn usually ~43000-45000us
    microsLastStartBitAutoSyn = now;
  microsLastFallingEdge = now;
}

void ebus::setupBusIsr(HardwareSerial* serial, const int8_t& rxPin,
                       const int8_t& txPin) {
  if (serial) {
    hwSerial = serial;
    hwSerial->begin(2400, SERIAL_8N1, rxPin, txPin);

    bus = new ebus::Bus(*(hwSerial));
    byteQueue = new ebus::Queue<uint8_t>();
    ebus::handler = new ebus::Handler(bus, ebus::DEFAULT_ADDRESS);
    ebus::serviceRunner = new ebus::ServiceRunner(*ebus::handler, *byteQueue);

    // Attach the ISR to the UART receive event
    hwSerial->onReceive(&onUartRx);

    // Request bus timer: one-shot, armed as needed
    uint32_t timer_clk = APB_CLK_FREQ;       // Usually 80 MHz
    uint16_t divider = timer_clk / 1000000;  // For 1us tick
    requestBusTimer = timerBegin(0, divider, true);
    timerAttachInterrupt(requestBusTimer, &onRequestBusTimer, true);
    timerAlarmDisable(requestBusTimer);

    // Attach the ISR to the GPIO event
    pinMode(rxPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(rxPin), &onFallingEdge, FALLING);
  }
}

void ebus::setRequestOffset(const uint16_t& offset) { requestOffset = offset; }
