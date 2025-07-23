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

#include "driver/uart.h"
#include "esp32-hal.h"

#define FALLING_EDGE_BUFFER_SIZE 5

// This value can be adjusted if the bus isr is not working as expected.
volatile uint16_t busIsrWindow = 4300;  // usually between 4300-4456 us
volatile uint16_t busIsrOffset = 80;    // mainly for context switch and write

volatile bool rxActivitySinceTimerArmedFlag = false;

volatile uint8_t bufferIndex = 0;  // index for falling edge buffer
volatile int64_t microsEdgeBuffer[FALLING_EDGE_BUFFER_SIZE] = {0};

volatile int64_t microsStartBit = 0;  // estimated start bit time

volatile bool busRequestedFlag = false;

volatile bool busIsrExpectedFlag = false;
volatile bool busIsrTimerFlag = false;

volatile bool microsBusIsrDelayFlag = false;
volatile bool microsBusIsrWindowFlag = false;

volatile int64_t microsLastDelay = 0;
volatile int64_t microsLastWindow = 0;

hw_timer_t* busIsrTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static uart_port_t busUartNum;
static QueueHandle_t uartEventQueue = nullptr;

ebus::Bus* bus = nullptr;
ebus::Queue<uint8_t>* byteQueue = nullptr;
ebus::Handler* ebus::handler = nullptr;
ebus::ServiceRunner* ebus::serviceRunner = nullptr;

// UART event task: reads UART events and pushes bytes to byteQueue
void ebusUartEventTask(void* pvParameters) {
  uart_event_t event;
  uint8_t data[128];
  for (;;) {
    if (xQueueReceive(uartEventQueue, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(busUartNum, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          // --- Begin onUartRx logic ---
          if (!byteQueue || !ebus::handler) continue;

          rxActivitySinceTimerArmedFlag = true;

          // Handle bus request logic only if needed
          if (byte == ebus::sym_syn && ebus::handler->busRequest()) {
            int64_t now = esp_timer_get_time();
            float bit_time = 1000000.0 / 2400.0;  // ~416.67 us

            // Calculate the expected start bit time based on the current time
            // and the bit time with a 0.5 bit offset.
            // Expected start bit time is calculated as:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - (int64_t)(9.5 * bit_time);

            // Get the start time of the last sync byte.
            // Due the nature of the sync byte (0xAA) and the hardware delays,
            // the interesting index is two positions ahead of the current
            // buffer index. This is because the sync byte is sent at the start
            // of the frame, and we want to align the start bit with the sync
            // byte. The bufferIndex is incremented in the onFallingEdge ISR. So
            // we need to access the bufferIndex + 2 position. This is the last
            // start bit index.
            uint8_t lastStartBitIndex =
                (bufferIndex + 2) % FALLING_EDGE_BUFFER_SIZE;
            portENTER_CRITICAL_ISR(&timerMux);
            microsStartBit = microsEdgeBuffer[lastStartBitIndex];
            portEXIT_CRITICAL_ISR(&timerMux);

            // Accept if the difference is within 1.5 bit times
            int64_t delta = (expected_start_bit_time > microsStartBit)
                                ? (expected_start_bit_time - microsStartBit)
                                : (microsStartBit - expected_start_bit_time);

            if (delta < (bit_time * 1.5f)) {
              int64_t microsSinceStartBit =
                  esp_timer_get_time() - microsStartBit;
              int64_t delay = busIsrWindow - microsSinceStartBit - busIsrOffset;

              if (delay < 0) delay = 0;

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
              busIsrExpectedFlag = true;
            }
          }

          // Push byte to queue (should be lock-free or ISR-safe)
          byteQueue->pushFromISR(byte);
          // --- End onUartRx logic ---
        }
      }
      // Handle other UART events if needed
    }
  }
}

// ISR: Save the falling edges in order to estimate the sync byte
void IRAM_ATTR onFallingEdge() {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timerMux);
  bufferIndex = (bufferIndex + 1) % FALLING_EDGE_BUFFER_SIZE;
  microsEdgeBuffer[bufferIndex] = now;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR: Write request byte at the exact time
void IRAM_ATTR onBusIsrTimer() {
  if (!rxActivitySinceTimerArmedFlag) {
    uint8_t byte = ebus::handler->getAddress();
    uart_write_bytes(busUartNum, (const char*)&byte, 1);  // ISR-safe write
    busRequestedFlag = true;
    portENTER_CRITICAL_ISR(&timerMux);
    microsLastWindow = esp_timer_get_time() - microsStartBit;
    portEXIT_CRITICAL_ISR(&timerMux);
    microsBusIsrWindowFlag = true;
  } else {
    busIsrTimerFlag = true;
  }
}

void ebus::setupBusIsr(const uart_port_t& uartNum, const int8_t& rxPin,
                       const int8_t& txPin, const uint8_t& timer) {
  busUartNum = uartNum;

  bus = new ebus::Bus(busUartNum);
  byteQueue = new ebus::Queue<uint8_t>();
  ebus::handler = new ebus::Handler(bus, ebus::DEFAULT_ADDRESS);
  ebus::serviceRunner = new ebus::ServiceRunner(*ebus::handler, *byteQueue);

  // UART configuration
  uart_config_t uart_config = {.baud_rate = 2400,
                               .data_bits = UART_DATA_8_BITS,
                               .parity = UART_PARITY_DISABLE,
                               .stop_bits = UART_STOP_BITS_1,
                               .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  uart_param_config(busUartNum, &uart_config);
  uart_set_pin(busUartNum, txPin, rxPin, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(busUartNum, 256, 256, 1, &uartEventQueue, 0);
  uart_set_rx_full_threshold(busUartNum, 1);
  uart_set_rx_timeout(busUartNum, 1);

  xTaskCreate(ebusUartEventTask, "ebusUartEventTask", 2048, NULL,
              configMAX_PRIORITIES - 1, NULL);

  // Attach the ISR to the GPIO event
  pinMode(rxPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(rxPin), &onFallingEdge, FALLING);

  // BusIsr timer: one-shot, armed as needed
  uint16_t divider = getApbFrequency() / 1000000;  // For 1us tick
  busIsrTimer = timerBegin(timer, divider, true);
  timerAttachInterrupt(busIsrTimer, &onBusIsrTimer, true);
  timerAlarmDisable(busIsrTimer);
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

  if (busIsrExpectedFlag) {
    busIsrExpectedFlag = false;
    ebus::handler->busIsrExpected();
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