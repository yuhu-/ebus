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

#if defined(ESP32)
#include "BusFreeRtos.hpp"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../Common.hpp"

ebus::BusFreeRtos::BusFreeRtos(const busConfig& config, Request* request)
    : uartPortNum(config.uart_port),
      rxPin(config.rx_pin),
      txPin(config.tx_pin),
      timerGroupNum(static_cast<timer_group_t>(config.timer_group)),
      timerIdxNum(static_cast<timer_idx_t>(config.timer_idx)),
      request(request) {
  byteQueue = new ebus::Queue<ebus::BusEvent>();

  configureUart();
  configureGpio();
  configureTimer();

  start();
}

ebus::BusFreeRtos::~BusFreeRtos() {
  stop();
  if (byteQueue) {
    delete byteQueue;
    byteQueue = nullptr;
  }
}

void ebus::BusFreeRtos::start() {
  xTaskCreate(&BusFreeRtos::s_ebusUartEventRunner, "ebusUartEventRunner", 2048,
              this, configMAX_PRIORITIES - 1, nullptr);
}

void ebus::BusFreeRtos::stop() {
  // best-effort cleanup
  if (uartEventQueue) {
    uart_driver_delete(uartPortNum);
    uartEventQueue = nullptr;
  }
  if (byteQueue) {
    delete byteQueue;
    byteQueue = nullptr;
  }
}

ebus::Queue<ebus::BusEvent>* ebus::BusFreeRtos::getQueue() const {
  return byteQueue;
}

void ebus::BusFreeRtos::writeByte(const uint8_t byte) {
  uart_write_bytes(uartPortNum, static_cast<const void*>(&byte), 1);
}

void ebus::BusFreeRtos::setWindow(const uint16_t window) {
  portENTER_CRITICAL_ISR(&timerMux);
  this->window = window;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void ebus::BusFreeRtos::setOffset(const uint16_t offset) {
  portENTER_CRITICAL_ISR(&timerMux);
  this->offset = offset;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void ebus::BusFreeRtos::resetCounter() {
#define X(name) counter.name = 0;
  EBUS_BUS_COUNTER_LIST
#undef X
}

const ebus::BusFreeRtos::Counter& ebus::BusFreeRtos::getCounter() const {
  return counter;
}

void ebus::BusFreeRtos::resetTiming() {
  busDelay.clear();
  busWindow.clear();
}

const ebus::BusFreeRtos::Timing& ebus::BusFreeRtos::getTiming() {
#define X(name)                          \
  {                                      \
    auto values = name.getValues();      \
    timing.name##Last = values.last;     \
    timing.name##Count = values.count;   \
    timing.name##Mean = values.mean;     \
    timing.name##StdDev = values.stddev; \
  }
  EBUS_BUS_TIMING_LIST
#undef X
  return timing;
}

void ebus::BusFreeRtos::configureUart() {
  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  // Setup UART configuration
  uart_param_config(uartPortNum, &uart_config);
  uart_set_pin(uartPortNum, txPin, rxPin, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(uartPortNum, 256, 256, 1, &uartEventQueue, 0);
  uart_set_rx_full_threshold(uartPortNum, 1);
  uart_set_rx_timeout(uartPortNum, 1);
}

void ebus::BusFreeRtos::configureGpio() {
  gpio_config_t gpio_conf = {
      .pin_bit_mask = (1ULL << rxPin),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };

  gpio_config(&gpio_conf);

  // Install GPIO ISR service
  gpio_install_isr_service(0);

  // Register the ISR handler
  gpio_isr_handler_add(static_cast<gpio_num_t>(rxPin), &s_onFallingEdge, this);
}

void ebus::BusFreeRtos::configureTimer() {
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

  // Register the ISR callback
  timer_isr_callback_add(timerGroupNum, timerIdxNum, &s_onBusIsrTimer, this,
                         ESP_INTR_FLAG_IRAM);
  timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_DIS);
}

// static trampoline for FreeRTOS task
void ebus::BusFreeRtos::s_ebusUartEventRunner(void* arg) {
  BusFreeRtos* self = static_cast<BusFreeRtos*>(arg);
  if (self) self->ebusUartEventRunner();
}

void ebus::BusFreeRtos::ebusUartEventRunner() {
  uart_event_t event;
  uint8_t data[128];
  for (;;) {
    if (xQueueReceive(uartEventQueue, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(uartPortNum, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          if (!byteQueue) continue;

          if (byte == ebus::sym_syn && request->busRequestPending()) {
            int64_t now = esp_timer_get_time();

            // Calculation of the expected start bit time based on the current
            // time and the bit time with a 0.5-bit offset. The expected start
            // bit time is calculated as follows:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - byte_time;

            // Retrieving the start time of the last sync byte. Due to the
            // nature of the sync byte (0xAA), the buffer size used, and
            // hardware delays, the relevant index is two positions before the
            // current buffer index. This is because the sync byte is sent at
            // the beginning of the frame, and we want to align the start bit
            // with the sync byte. The buffer index is incremented in the
            // onFallingEdge ISR. Therefore, we need to access the position
            // bufferIndex + 2. This is the index of the last start bit.
            portENTER_CRITICAL_ISR(&timerMux);
            microsStartBit =
                microsEdgeBuffer[(bufferIndex + 2) % FALLING_EDGE_BUFFER_SIZE];
            portEXIT_CRITICAL_ISR(&timerMux);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // startBitFlag to true.
            int64_t delta = std::abs(expected_start_bit_time - microsStartBit);

            if (delta < static_cast<int64_t>(bit_time * 1.5f)) {
              int64_t microsSinceStartBit =
                  esp_timer_get_time() - microsStartBit;
              int64_t delay = window - microsSinceStartBit - offset;
              if (delay < 0) delay = 0;

              timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_DIS);
              timer_set_counter_value(timerGroupNum, timerIdxNum, 0);
              timer_set_alarm_value(timerGroupNum, timerIdxNum, delay);
              timer_set_auto_reload(timerGroupNum, timerIdxNum,
                                    TIMER_AUTORELOAD_DIS);
              timer_set_alarm(timerGroupNum, timerIdxNum, TIMER_ALARM_EN);

              portENTER_CRITICAL_ISR(&timerMux);
              microsLastDelay = delay;
              microsDelayFlag = true;
              portEXIT_CRITICAL_ISR(&timerMux);
            } else {
              portENTER_CRITICAL_ISR(&timerMux);
              startBitFlag = true;
              portEXIT_CRITICAL_ISR(&timerMux);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          ebus::BusEvent busEvent;
          busEvent.byte = byte;
          portENTER_CRITICAL_ISR(&timerMux);
          busEvent.busRequest = busRequestFlag;
          busEvent.startBit = startBitFlag;
          if (microsDelayFlag) busDelay.addDuration(microsLastDelay);
          if (microsWindowFlag) busWindow.addDuration(microsLastWindow);
          // clear the global flags we consumed
          busRequestFlag = false;
          startBitFlag = false;
          portEXIT_CRITICAL_ISR(&timerMux);

          if (byteQueue) byteQueue->push(busEvent);
        }
      }
    }
  }
}

// static ISR trampoline -> instance method
void IRAM_ATTR ebus::BusFreeRtos::s_onFallingEdge(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  if (inst) inst->onFallingEdge();
}

void IRAM_ATTR ebus::BusFreeRtos::onFallingEdge() {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timerMux);
  bufferIndex = (bufferIndex + 1) % FALLING_EDGE_BUFFER_SIZE;
  microsEdgeBuffer[bufferIndex] = now;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// static ISR trampoline -> instance method
bool IRAM_ATTR ebus::BusFreeRtos::s_onBusIsrTimer(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  return inst ? inst->onBusIsrTimer() : false;
}

bool IRAM_ATTR ebus::BusFreeRtos::onBusIsrTimer() {
  uint8_t byte = request->busRequestAddress();
  uart_write_bytes(uartPortNum, static_cast<const void*>(&byte), 1);
  portENTER_CRITICAL_ISR(&timerMux);
  microsLastWindow = esp_timer_get_time() - microsStartBit;
  busRequestFlag = true;
  microsWindowFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
  return false;
}

#endif
