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

ebus::BusFreeRtos::BusFreeRtos(bus_config_t& config, Request& request)
    : m_uartPortNum(config.uart_port),
      m_rxPin(config.rx_pin),
      m_txPin(config.tx_pin),
      m_timerGroupNum(static_cast<timer_group_t>(config.timer_group)),
      m_timerIdxNum(static_cast<timer_idx_t>(config.timer_idx)),
      m_request(request) {
  m_byteQueue = new ebus::Queue<ebus::BusEvent>();

  configureUart();
  configureGpio();
  configureTimer();

  start();
}

ebus::BusFreeRtos::~BusFreeRtos() {
  stop();
  if (m_byteQueue) {
    delete m_byteQueue;
    m_byteQueue = nullptr;
  }
}

void ebus::BusFreeRtos::start() {
  xTaskCreate(&BusFreeRtos::s_ebusUartEventRunner, "ebusUartEventRunner", 2048,
              this, configMAX_PRIORITIES - 1, nullptr);
}

void ebus::BusFreeRtos::stop() {
  // best-effort cleanup
  if (m_uartEventQueue) {
    uart_driver_delete(m_uartPortNum);
    m_uartEventQueue = nullptr;
  }
  if (m_byteQueue) {
    delete m_byteQueue;
    m_byteQueue = nullptr;
  }
}

ebus::Queue<ebus::BusEvent>* ebus::BusFreeRtos::getQueue() const {
  return m_byteQueue;
}

void ebus::BusFreeRtos::writeByte(const uint8_t byte) {
  uart_write_bytes(m_uartPortNum, static_cast<const void*>(&byte), 1);
}

void ebus::BusFreeRtos::setWindow(const uint16_t window) {
  portENTER_CRITICAL_ISR(&m_timerMux);
  m_busIsrWindow = window;
  portEXIT_CRITICAL_ISR(&m_timerMux);
}

void ebus::BusFreeRtos::setOffset(const uint16_t offset) {
  portENTER_CRITICAL_ISR(&m_timerMux);
  m_busIsrOffset = offset;
  portEXIT_CRITICAL_ISR(&m_timerMux);
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
  busIsrDelay.clear();
  busIsrWindow.clear();
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
  uart_param_config(m_uartPortNum, &uart_config);
  uart_set_pin(m_uartPortNum, m_txPin, m_rxPin, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(m_uartPortNum, 256, 256, 1, &m_uartEventQueue, 0);
  uart_set_rx_full_threshold(m_uartPortNum, 1);
  uart_set_rx_timeout(m_uartPortNum, 1);
}

void ebus::BusFreeRtos::configureGpio() {
  gpio_config_t gpio_conf = {
      .pin_bit_mask = (1ULL << m_rxPin),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };

  gpio_config(&gpio_conf);

  // Install GPIO ISR service
  gpio_install_isr_service(0);

  // Register the ISR handler
  gpio_isr_handler_add(static_cast<gpio_num_t>(m_rxPin), &s_onFallingEdge,
                       this);
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
  timer_init(m_timerGroupNum, m_timerIdxNum, &timer_config);
  timer_set_counter_value(m_timerGroupNum, m_timerIdxNum, 0);
  timer_start(m_timerGroupNum, m_timerIdxNum);

  // Register the ISR callback
  timer_isr_callback_add(m_timerGroupNum, m_timerIdxNum, &s_onBusIsrTimer, this,
                         ESP_INTR_FLAG_IRAM);
  timer_set_alarm(m_timerGroupNum, m_timerIdxNum, TIMER_ALARM_DIS);
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
    if (xQueueReceive(m_uartEventQueue, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(m_uartPortNum, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          if (!m_byteQueue) continue;

          if (byte == ebus::sym_syn && m_request.busRequestPending()) {
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
            portENTER_CRITICAL_ISR(&m_timerMux);
            m_microsStartBit = m_microsEdgeBuffer[(m_bufferIndex + 2) %
                                                  FALLING_EDGE_BUFFER_SIZE];
            portEXIT_CRITICAL_ISR(&m_timerMux);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // startBitFlag to true.
            int64_t delta =
                std::abs(expected_start_bit_time - m_microsStartBit);

            if (delta < static_cast<int64_t>(bit_time * 1.5f)) {
              int64_t microsSinceStartBit =
                  esp_timer_get_time() - m_microsStartBit;
              int64_t delay =
                  m_busIsrWindow - microsSinceStartBit - m_busIsrOffset;
              if (delay < 0) delay = 0;

              timer_set_alarm(m_timerGroupNum, m_timerIdxNum, TIMER_ALARM_DIS);
              timer_set_counter_value(m_timerGroupNum, m_timerIdxNum, 0);
              timer_set_alarm_value(m_timerGroupNum, m_timerIdxNum, delay);
              timer_set_auto_reload(m_timerGroupNum, m_timerIdxNum,
                                    TIMER_AUTORELOAD_DIS);
              timer_set_alarm(m_timerGroupNum, m_timerIdxNum, TIMER_ALARM_EN);

              portENTER_CRITICAL_ISR(&m_timerMux);
              m_microsLastDelay = delay;
              m_microsDelayFlag = true;
              portEXIT_CRITICAL_ISR(&m_timerMux);
            } else {
              portENTER_CRITICAL_ISR(&m_timerMux);
              m_startBitFlag = true;
              portEXIT_CRITICAL_ISR(&m_timerMux);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          ebus::BusEvent busEvent;
          busEvent.byte = byte;
          portENTER_CRITICAL_ISR(&m_timerMux);
          busEvent.busRequest = m_busRequestFlag;
          busEvent.startBit = m_startBitFlag;
          if (m_microsDelayFlag) busIsrDelay.addDuration(m_microsLastDelay);
          if (m_microsWindowFlag) busIsrWindow.addDuration(m_microsLastWindow);
          // busEvent.microsDelay = m_microsDelayFlag;
          // busEvent.microsLastDelay = m_microsLastDelay;
          // busEvent.microsWindow = m_microsWindowFlag;
          // busEvent.microsLastWindow = m_microsLastWindow;
          // clear the global flags we consumed
          m_busRequestFlag = false;
          m_startBitFlag = false;
          // m_microsDelayFlag = false;
          // m_microsWindowFlag = false;
          // m_microsLastDelay = 0;
          // m_microsLastWindow = 0;
          portEXIT_CRITICAL_ISR(&m_timerMux);

          if (m_byteQueue) m_byteQueue->push(busEvent);
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
  portENTER_CRITICAL_ISR(&m_timerMux);
  m_bufferIndex = (m_bufferIndex + 1) % FALLING_EDGE_BUFFER_SIZE;
  m_microsEdgeBuffer[m_bufferIndex] = now;
  portEXIT_CRITICAL_ISR(&m_timerMux);
}

// static ISR trampoline -> instance method
bool IRAM_ATTR ebus::BusFreeRtos::s_onBusIsrTimer(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  return inst ? inst->onBusIsrTimer() : false;
}

bool IRAM_ATTR ebus::BusFreeRtos::onBusIsrTimer() {
  uint8_t byte = m_request.busRequestAddress();
  uart_write_bytes(m_uartPortNum, static_cast<const void*>(&byte), 1);
  portENTER_CRITICAL_ISR(&m_timerMux);
  m_microsLastWindow = esp_timer_get_time() - m_microsStartBit;
  m_busRequestFlag = true;
  m_microsWindowFlag = true;
  portEXIT_CRITICAL_ISR(&m_timerMux);
  return false;
}

#endif
