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

#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/uart.h"

constexpr uint8_t FALLING_EDGE_BUFFER_SIZE = 5;

// This value can be adjusted if the bus isr is not working as expected.
volatile uint16_t busIsrWindow = 4300;  // usually between 4300-4456 us
volatile uint16_t busIsrOffset = 80;    // mainly for context switch and write

volatile uint8_t bufferIndex = 0;  // index for falling edge buffer
volatile int64_t microsEdgeBuffer[FALLING_EDGE_BUFFER_SIZE] = {0};

volatile int64_t microsStartBit = 0;  // estimated start bit time

volatile bool busRequestedFlag = false;
volatile bool busIsrStartBitFlag = false;

volatile bool microsBusIsrDelayFlag = false;
volatile bool microsBusIsrWindowFlag = false;

volatile int64_t microsLastDelay = 0;
volatile int64_t microsLastWindow = 0;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static uart_port_t uartPortNum = UART_NUM_1;
static QueueHandle_t uartEventQueue = nullptr;

static timer_group_t timerGroupNum = TIMER_GROUP_1;
static timer_idx_t timerIdxNum = TIMER_0;

ebus::Bus* bus = nullptr;
ebus::Queue<uint8_t>* byteQueue = nullptr;
ebus::Handler* ebus::handler = nullptr;
ebus::ServiceRunner* ebus::serviceRunner = nullptr;

// UART event task: reads UART events and pushes bytes to byteQueue
void ebusUartEventTask(void* arg) {
  uart_event_t event;
  uint8_t data[128];
  for (;;) {
    if (xQueueReceive(uartEventQueue, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(uartPortNum, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          // --- Begin onUartRx logic ---
          if (!byteQueue || !ebus::handler) continue;

          // Handle bus request logic only if needed
          if (byte == ebus::sym_syn && ebus::handler->busRequest()) {
            int64_t now = esp_timer_get_time();
            float bit_time = 1000000.0 / 2400.0;  // ~416.67 us

            // Calculation of the expected start bit time based on the current
            // time and the bit time with a 0.5-bit offset. The expected start
            // bit time is calculated as follows:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - (int64_t)(9.5 * bit_time);

            // Retrieving the start time of the last sync byte. Due to the
            // nature of the sync byte (0xAA), the buffer size used, and
            // hardware delays, the relevant index is two positions before the
            // current buffer index. This is because the sync byte is sent at
            // the beginning of the frame, and we want to align the start bit
            // with the sync byte. The buffer index is incremented in the
            // onFallingEdge ISR. Therefore, we need to access the position
            // bufferIndex + 2. This is the index of the last start bit.
            uint8_t lastStartBitIndex =
                (bufferIndex + 2) % FALLING_EDGE_BUFFER_SIZE;
            portENTER_CRITICAL_ISR(&timerMux);
            microsStartBit = microsEdgeBuffer[lastStartBitIndex];
            portEXIT_CRITICAL_ISR(&timerMux);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // busIsrStartBitFlag to true.
            int64_t delta = (expected_start_bit_time > microsStartBit)
                                ? (expected_start_bit_time - microsStartBit)
                                : (microsStartBit - expected_start_bit_time);

            if (delta < (bit_time * 1.5f)) {
              int64_t microsSinceStartBit =
                  esp_timer_get_time() - microsStartBit;
              int64_t delay = busIsrWindow - microsSinceStartBit - busIsrOffset;

              if (delay < 0) delay = 0;

              timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_DIS);
              timer_set_counter_value(timerGroupNum, timerIdxNum, 0);
              timer_set_alarm_value(timerGroupNum, timerIdxNum, delay);
              timer_set_auto_reload(timerGroupNum, timerIdxNum,
                                    TIMER_AUTORELOAD_DIS);
              timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_EN);

              portENTER_CRITICAL_ISR(&timerMux);
              microsLastDelay = delay;
              portEXIT_CRITICAL_ISR(&timerMux);
              microsBusIsrDelayFlag = true;
            } else {
              busIsrStartBitFlag = true;
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
void IRAM_ATTR onFallingEdge(void* arg) {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timerMux);
  bufferIndex = (bufferIndex + 1) % FALLING_EDGE_BUFFER_SIZE;
  microsEdgeBuffer[bufferIndex] = now;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR: Write request byte at the exact time
bool IRAM_ATTR onBusIsrTimer(void* arg) {
  uint8_t byte = ebus::handler->getAddress();
  uart_write_bytes(uartPortNum, static_cast<const void*>(&byte), 1);
  busRequestedFlag = true;
  portENTER_CRITICAL_ISR(&timerMux);
  microsLastWindow = esp_timer_get_time() - microsStartBit;
  portEXIT_CRITICAL_ISR(&timerMux);
  microsBusIsrWindowFlag = true;
  return false;  // Do not yield
}

void ebus::setupBusIsr(const uint8_t& uartPort, const uint8_t& rxPin,
                       const uint8_t& txPin, const uint8_t& timerGroup,
                       const uint8_t& timerIdx) {
  uartPortNum = uartPort;
  timerGroupNum = static_cast<timer_group_t>(timerGroup);
  timerIdxNum = static_cast<timer_idx_t>(timerIdx);

  bus = new ebus::Bus(uartPortNum);
  byteQueue = new ebus::Queue<uint8_t>();
  handler = new ebus::Handler(bus, DEFAULT_ADDRESS);
  serviceRunner = new ebus::ServiceRunner(*handler, *byteQueue);

  // UART configuration
  uart_config_t uart_config = {.baud_rate = 2400,
                               .data_bits = UART_DATA_8_BITS,
                               .parity = UART_PARITY_DISABLE,
                               .stop_bits = UART_STOP_BITS_1,
                               .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  uart_param_config(uartPortNum, &uart_config);
  uart_set_pin(uartPortNum, txPin, rxPin, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(uartPortNum, 256, 256, 1, &uartEventQueue, 0);
  uart_set_rx_full_threshold(uartPortNum, 1);
  uart_set_rx_timeout(uartPortNum, 1);

  // Create the UART event task
  xTaskCreate(ebusUartEventTask, "ebusUartEventTask", 2048, NULL,
              configMAX_PRIORITIES - 1, NULL);

  // Attach the ISR to the GPIO event
  gpio_config_t gpio_conf = {.pin_bit_mask = (1ULL << rxPin),
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_NEGEDGE};
  gpio_config(&gpio_conf);

  // Install GPIO ISR service
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)rxPin, onFallingEdge, nullptr);

  // Create a Timer configuration for the one-shot timer
  timer_config_t timer_config = {
      .alarm_en = TIMER_ALARM_DIS,
      .counter_en = TIMER_PAUSE,
      .intr_type = TIMER_INTR_LEVEL,
      .counter_dir = TIMER_COUNT_UP,
      .auto_reload = TIMER_AUTORELOAD_DIS,
      .divider = TIMER_BASE_CLK / 1000000,  // 80 for 1us tick at 80MHz
  };

  // Initialize the timer
  timer_init(timerGroupNum, timerIdxNum, &timer_config);
  timer_set_counter_value(timerGroupNum, timerIdxNum, 0);
  timer_start(timerGroupNum, timerIdxNum);

  // Register the timer ISR callback
  timer_isr_callback_add(timerGroupNum, timerIdxNum, onBusIsrTimer, nullptr,
                         ESP_INTR_FLAG_IRAM);
  timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_DIS);
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
  if (busRequestedFlag) {
    busRequestedFlag = false;
    ebus::handler->busRequested();
  }

  if (busIsrStartBitFlag) {
    busIsrStartBitFlag = false;
    ebus::handler->busIsrStartBit();
  }

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
}