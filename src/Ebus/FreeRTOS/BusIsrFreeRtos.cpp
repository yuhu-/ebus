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

hw_timer_t* busIsrTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


// This value can be adjusted if the bus isr is not working as expected.
volatile uint16_t busIsrWindow = 4300;  // usually between 4300-4456 us
volatile uint16_t busIsrOffset = 80;    // mainly for context switch and write

volatile bool rxActivitySinceTimerArmedFlag = false;

volatile uint32_t microsLastFallingEdge = 0;
volatile uint32_t microsLastStartBitAutoSyn = 0;

volatile bool busRequestedFlag = false;

volatile bool busIsrDelayMinFlag = false;
volatile bool busIsrDelayMaxFlag = false;
volatile bool busIsrTimerFlag = false;

volatile bool microsBusIsrDelayFlag = false;
volatile bool microsBusIsrWindowFlag = false;

volatile int64_t microsLastDelay = 0;
volatile int64_t microsLastWindow = 0;

static HardwareSerial* hwSerial = nullptr;

ebus::Bus* bus = nullptr;
ebus::Queue<uint8_t>* byteQueue = nullptr;
ebus::Handler* ebus::handler = nullptr;
ebus::ServiceRunner* ebus::serviceRunner = nullptr;

// ISR: Save the timestamp of the first bit (falling edge) from AutoSyn
void IRAM_ATTR onFallingEdge() {
  uint32_t now = micros();
  portENTER_CRITICAL_ISR(&timerMux);
  uint32_t duration = now - microsLastFallingEdge;
  if (duration > 35000)  // AutoSyn usually takes ~43000-45000us
    microsLastStartBitAutoSyn = now;
  microsLastFallingEdge = now;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR: Fires when a byte is received
void IRAM_ATTR onUartRx() {
  if (!byteQueue || !ebus::handler) return;

  rxActivitySinceTimerArmedFlag = true;

  // Read all available bytes as quickly as possible
  while (hwSerial->available()) {
    uint8_t byte = hwSerial->read();

    // Handle bus request logic only if needed
    if (byte == ebus::sym_syn && ebus::handler->busRequest()) {
      portENTER_CRITICAL_ISR(&timerMux);
      uint32_t microsSinceLastStartBit = micros() - microsLastStartBitAutoSyn;
      int64_t delay = busIsrWindow - microsSinceLastStartBit - busIsrOffset;
      portEXIT_CRITICAL_ISR(&timerMux);
      if (delay < 0) {
        busIsrDelayMinFlag = true;
      } else if (delay < 500) {
        rxActivitySinceTimerArmedFlag = false;
        timerAlarmDisable(busIsrTimer);
        timerWrite(busIsrTimer, 0);
        timerAlarmWrite(busIsrTimer, delay, false);
        timerAlarmEnable(busIsrTimer);
        portENTER_CRITICAL_ISR(&timerMux);
        microsLastDelay = delay;
        portEXIT_CRITICAL_ISR(&timerMux);
        microsBusIsrDelayFlag = true;
      } else {
        busIsrDelayMaxFlag = true;
      }
    }

    // Push byte to queue (should be lock-free or ISR-safe)
    byteQueue->pushFromISR(byte);
  }
}

// ISR: Write request byte at the exact time
void IRAM_ATTR onBusIsrTimer() {
  if (!rxActivitySinceTimerArmedFlag) {
    hwSerial->write(ebus::handler->getAddress());
    busRequestedFlag = true;
    portENTER_CRITICAL_ISR(&timerMux);
    microsLastWindow = micros() - microsLastStartBitAutoSyn;
    portEXIT_CRITICAL_ISR(&timerMux);
    microsBusIsrWindowFlag = true;
  } else {
    busIsrTimerFlag = true;
  }
}

void ebus::setupBusIsr(HardwareSerial* serial, const int8_t& rxPin,
                       const int8_t& txPin, const uint8_t& timer) {
  if (serial) {
    hwSerial = serial;
    hwSerial->begin(2400, SERIAL_8N1, rxPin, txPin);

    bus = new ebus::Bus(*(hwSerial));
    byteQueue = new ebus::Queue<uint8_t>();
    ebus::handler = new ebus::Handler(bus, ebus::DEFAULT_ADDRESS);
    ebus::serviceRunner = new ebus::ServiceRunner(*ebus::handler, *byteQueue);

    // Attach the ISR to the UART receive event
    hwSerial->onReceive(&onUartRx);

    // BusIsr timer: one-shot, armed as needed
    uint16_t divider = getApbFrequency() / 1000000;  // For 1us tick
    busIsrTimer = timerBegin(timer, divider, true);
    timerAttachInterrupt(busIsrTimer, &onBusIsrTimer, true);
    timerAlarmDisable(busIsrTimer);

    // Attach the ISR to the GPIO event
    pinMode(rxPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(rxPin), &onFallingEdge, FALLING);
  }
}

void ebus::setBusIsrWindow(const uint16_t& window) {
  portENTER_CRITICAL_ISR(&timerMux);
  busIsrWindow = window;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void ebus::setBusIsrOffset(const uint16_t& offset) {
  portENTER_CRITICAL_ISR(&timerMux);
  busIsrOffset = offset;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void ebus::processBusIsrEvents() {
  if (microsBusIsrDelayFlag) {
    microsBusIsrDelayFlag = false;
    int64_t safeDelay;
    portENTER_CRITICAL_ISR(&timerMux);
    safeDelay = microsLastDelay;
    portEXIT_CRITICAL_ISR(&timerMux);
    ebus::handler->microsBusIsrDelay(safeDelay);
  }

  if (microsBusIsrWindowFlag) {
    microsBusIsrWindowFlag = false;
    int64_t safeWindow;
    portENTER_CRITICAL_ISR(&timerMux);
    safeWindow = microsLastWindow;
    portEXIT_CRITICAL_ISR(&timerMux);
    ebus::handler->microsBusIsrWindow(safeWindow);
  }

  if (busIsrDelayMinFlag) {
    busIsrDelayMinFlag = false;
    ebus::handler->busIsrDelayMin();
  }

  if (busIsrDelayMaxFlag) {
    busIsrDelayMaxFlag = false;
    ebus::handler->busIsrDelayMax();
  }

  if (busIsrTimerFlag) {
    busIsrTimerFlag = false;
    ebus::handler->busIsrTimer();
  }

  if (busRequestedFlag) {
    busRequestedFlag = false;
    ebus::handler->busRequested();
  }
}