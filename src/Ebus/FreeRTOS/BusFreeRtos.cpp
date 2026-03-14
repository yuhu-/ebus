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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_clk_tree.h"
#else
#include "esp32c3/clk.h"
#endif

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../Common.hpp"
#include "driver/gpio.h"

ebus::BusFreeRtos::BusFreeRtos(const busConfig& config, Request* request)
    : uartPortNum_(static_cast<uart_port_t>(config.uart_port)),
      rxPin_(config.rx_pin),
      txPin_(config.tx_pin),
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
      timerGroupNum_(static_cast<timer_group_t>(config.timer_group)),
      timerIdxNum_(static_cast<timer_idx_t>(config.timer_idx)),
#endif
      request_(request) {
  byteQueue_ = new ebus::Queue<ebus::BusEvent>();

  configureUart();
  configureGpio();
  configureTimer();

  start();
}

ebus::BusFreeRtos::~BusFreeRtos() {
  stop();
  if (byteQueue_) {
    delete byteQueue_;
    byteQueue_ = nullptr;
  }
}

void ebus::BusFreeRtos::start() {
  xTaskCreate(&BusFreeRtos::s_ebusUartEventRunner, "ebusUartEventRunner", 2048,
              this, configMAX_PRIORITIES - 1, nullptr);
}

void ebus::BusFreeRtos::stop() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (gptimer) {
    gptimer_stop(gptimer);
    gptimer_disable(gptimer);
    gptimer_del_timer(gptimer);
    gptimer = nullptr;
  }
#else
  timer_pause(timerGroupNum_, timerIdxNum_);
#endif

  if (uartEventQueue_) {
    uart_driver_delete(uartPortNum_);
    uartEventQueue_ = nullptr;
  }
  if (byteQueue_) {
    delete byteQueue_;
    byteQueue_ = nullptr;
  }
}

ebus::Queue<ebus::BusEvent>* ebus::BusFreeRtos::getQueue() const {
  return byteQueue_;
}

void ebus::BusFreeRtos::writeByte(const uint8_t byte) {
  uart_write_bytes(uartPortNum_, static_cast<const void*>(&byte), 1);
}

void ebus::BusFreeRtos::setWindow(const uint16_t window) {
  portENTER_CRITICAL_ISR(&timerMux_);
  window_ = window;
  portEXIT_CRITICAL_ISR(&timerMux_);
}

void ebus::BusFreeRtos::setOffset(const uint16_t offset) {
  portENTER_CRITICAL_ISR(&timerMux_);
  offset_ = offset;
  portEXIT_CRITICAL_ISR(&timerMux_);
}

void ebus::BusFreeRtos::resetCounter() {
#define X(name) counter_.name = 0;
  EBUS_BUS_COUNTER_LIST
#undef X
}

const ebus::BusFreeRtos::Counter& ebus::BusFreeRtos::getCounter() const {
  return counter_;
}

void ebus::BusFreeRtos::resetTiming() {
  busDelay_.clear();
  busWindow_.clear();
}

const ebus::BusFreeRtos::Timing& ebus::BusFreeRtos::getTiming() {
#define X(name)                           \
  {                                       \
    auto values = name.getValues();       \
    timing_.name##Last = values.last;     \
    timing_.name##Count = values.count;   \
    timing_.name##Mean = values.mean;     \
    timing_.name##StdDev = values.stddev; \
  }
  EBUS_BUS_TIMING_LIST
#undef X
  return timing_;
}

void ebus::BusFreeRtos::configureUart() {
  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 0, 0)
      .rx_flow_ctrl_thresh = 0,
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#ifdef UART_SCLK_DEFAULT
      .source_clk = UART_SCLK_DEFAULT,
#else
      .source_clk = UART_SCLK_APB,
#endif
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      .flags =
          {
              .allow_pd = false,
              .backup_before_sleep = false,
          },
#endif
  };

  // Setup UART configuration
  uart_param_config(uartPortNum_, &uart_config);
  uart_set_pin(uartPortNum_, txPin_, rxPin_, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(uartPortNum_, 256, 256, 1, &uartEventQueue_, 0);
  uart_set_rx_full_threshold(uartPortNum_, 1);
  uart_set_rx_timeout(uartPortNum_, 1);
}

void ebus::BusFreeRtos::configureGpio() {
  gpio_config_t gpio_conf = {
      .pin_bit_mask = (1ULL << rxPin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };

  gpio_config(&gpio_conf);

  // Install GPIO ISR service with flags for IRAM and high priority (level 3)
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);

  // Register the ISR handler
  gpio_isr_handler_add(static_cast<gpio_num_t>(rxPin_), &s_onFallingEdge, this);
}

void ebus::BusFreeRtos::configureTimer() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  gptimer_config_t gpt_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,  // 1 Tick = 1 µs
      .intr_priority = 3,        //  high priority (1-3)
      .flags = {
          .intr_shared = false,  // do not share interrupt - reduce jitter
          .allow_pd = false,
          .backup_before_sleep = false,
      }};

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config, &gptimer));

  gptimer_event_callbacks_t cbs = {
      .on_alarm = s_onBusIsrTimer,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(gptimer));

#else
  timer_config_t timer_config = {
      .alarm_en = TIMER_ALARM_DIS,
      .counter_en = TIMER_PAUSE,
      .intr_type = TIMER_INTR_LEVEL,
      .counter_dir = TIMER_COUNT_UP,
      .auto_reload = TIMER_AUTORELOAD_DIS,
      .divider = static_cast<uint32_t>(esp_clk_apb_freq() / 1000000U),

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#ifdef TIMER_SRC_CLK_DEFAULT
      .clk_src = TIMER_SRC_CLK_DEFAULT,
#else
      .clk_src = TIMER_SRC_CLK_APB,
#endif
#endif
  };

  // Initialize the timer
  timer_init(timerGroupNum_, timerIdxNum_, &timer_config);
  timer_set_counter_value(timerGroupNum_, timerIdxNum_, 0);

  // Register the ISR callback with flags for IRAM and high priority (level 3)
  timer_isr_callback_add(timerGroupNum_, timerIdxNum_, s_onBusIsrTimer, this,
                         ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  timer_start(timerGroupNum_, timerIdxNum_);
#endif
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
    if (xQueueReceive(uartEventQueue_, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(uartPortNum_, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          if (!byteQueue_) continue;

          if (byte == sym_syn && request_->busRequestPending()) {
            int64_t now = esp_timer_get_time();

            // Calculation of the expected start bit time based on the current
            // time and the bit time with a 0.5-bit offset. The expected start
            // bit time is calculated as follows:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - byteTime_;

            // Retrieving the start time of the last sync byte. Due to the
            // nature of the sync byte (0xAA), the buffer size used, and
            // hardware delays, the relevant index is two positions before the
            // current buffer index. This is because the sync byte is sent at
            // the beginning of the frame, and we want to align the start bit
            // with the sync byte. The buffer index is incremented in the
            // onFallingEdge ISR. Therefore, we need to access the position
            // bufferIndex + 2. This is the index of the last start bit.
            portENTER_CRITICAL_ISR(&timerMux_);
            microsStartBit_ = microsEdgeBuffer_[(bufferIndex_ + 2) %
                                                FALLING_EDGE_BUFFER_SIZE];
            portEXIT_CRITICAL_ISR(&timerMux_);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // startBitFlag_ to true.
            int64_t delta = std::abs(expected_start_bit_time - microsStartBit_);

            if (delta < static_cast<int64_t>(bitTime_ * 1.5f)) {
              int64_t microsSinceStartBit =
                  esp_timer_get_time() - microsStartBit_;
              int64_t delay = window_ - microsSinceStartBit - offset_;
              if (delay < 0) delay = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
              gptimer_alarm_config_t alarm_config = {
                  .alarm_count = (uint64_t)delay,
                  .reload_count = 0,
                  .flags = {.auto_reload_on_alarm = false}};
              gptimer_stop(gptimer_);
              gptimer_set_raw_count(gptimer_, 0);
              gptimer_set_alarm_action(gptimer_, &alarm_config);
              gptimer_start(gptimer_);
#else
              // Legacy Timer Alarm setzen (v4.x)
              timer_set_counter_value(timerGroupNum_, timerIdxNum_, 0);
              timer_set_alarm_value(timerGroupNum_, timerIdxNum_,
                                    (double)delay);
              timer_set_alarm(timerGroupNum_, timerIdxNum_, TIMER_ALARM_EN);
#endif

              portENTER_CRITICAL_ISR(&timerMux_);
              microsLastDelay_ = delay;
              microsDelayFlag_ = true;
              portEXIT_CRITICAL_ISR(&timerMux_);
            } else {
              portENTER_CRITICAL_ISR(&timerMux_);
              startBitFlag_ = true;
              portEXIT_CRITICAL_ISR(&timerMux_);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          BusEvent busEvent;
          busEvent.byte = byte;
          portENTER_CRITICAL_ISR(&timerMux_);
          busEvent.busRequest = busRequestFlag_;
          busEvent.startBit = startBitFlag_;
          if (microsDelayFlag_) busDelay_.addDuration(microsLastDelay_);
          if (microsWindowFlag_) busWindow_.addDuration(microsLastWindow_);
          // clear the global flags we consumed
          busRequestFlag_ = false;
          startBitFlag_ = false;
          portEXIT_CRITICAL_ISR(&timerMux_);

          if (byteQueue_) byteQueue_->push(busEvent);
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

void ebus::BusFreeRtos::onFallingEdge() {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timerMux_);
  bufferIndex_ = (bufferIndex_ + 1) % FALLING_EDGE_BUFFER_SIZE;
  microsEdgeBuffer_[bufferIndex_] = now;
  portEXIT_CRITICAL_ISR(&timerMux_);
}

// static ISR trampoline -> instance method
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
bool IRAM_ATTR ebus::BusFreeRtos::s_onBusIsrTimer(
    gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata,
    void* user_ctx) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(user_ctx);
  return inst ? inst->onBusIsrTimer() : false;
}
#else
bool IRAM_ATTR ebus::BusFreeRtos::s_onBusIsrTimer(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  return inst ? inst->onBusIsrTimer() : false;
}
#endif

bool ebus::BusFreeRtos::onBusIsrTimer() {
  uint8_t byte = request_->busRequestAddress();
  uart_write_bytes(uartPortNum_, static_cast<const void*>(&byte), 1);
  portENTER_CRITICAL_ISR(&timerMux_);
  microsLastWindow_ = esp_timer_get_time() - microsStartBit_;
  busRequestFlag_ = true;
  microsWindowFlag_ = true;
  portEXIT_CRITICAL_ISR(&timerMux_);
  return false;
}

#endif
