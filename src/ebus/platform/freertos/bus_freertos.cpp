/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if defined(ESP32)
#include "platform/freertos/bus_freertos.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_clk_tree.h"
#else
#include "esp32c3/clk.h"
#endif

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/uart_ll.h>

#include "driver/gpio.h"
#include "utils/common.hpp"

ebus::BusFreeRtos::BusFreeRtos(const BusConfig& config,
                               const RuntimeConfig& runtime, Request* request)
    : uart_port_num_(static_cast<uart_port_t>(config.uart_port)),
      rx_pin_(config.rx_pin),
      tx_pin_(config.tx_pin),
      runtime_(runtime),
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
      timer_group_num_(static_cast<timer_group_t>(config.timer_group)),
      timer_idx_num_(static_cast<timer_idx_t>(config.timer_idx)),
#endif
      request_(request) {
  byte_queue_ = new ebus::Queue<ebus::BusEvent>();

  configureUart();
  configureGpio();
  configureTimer();

  start();
}

ebus::BusFreeRtos::~BusFreeRtos() {
  stop();
  if (byte_queue_) {
    delete byte_queue_;
    byte_queue_ = nullptr;
  }
}

void ebus::BusFreeRtos::start() {
  xTaskCreate(&BusFreeRtos::s_ebusUartEventRunner, "ebusUartEventRunner", 2048,
              this, configMAX_PRIORITIES - 1, nullptr);
}

void ebus::BusFreeRtos::stop() {
  gpio_isr_handler_remove(static_cast<gpio_num_t>(rx_pin_));
  gpio_intr_disable(static_cast<gpio_num_t>(rx_pin_));
  gpio_set_intr_type(static_cast<gpio_num_t>(rx_pin_), GPIO_INTR_DISABLE);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  if (gp_timer_) {
    gptimer_stop(gp_timer_);
    gptimer_disable(gp_timer_);
    gptimer_del_timer(gp_timer_);
    gp_timer_ = nullptr;
  }
#else
  timer_pause(timer_group_num_, timer_idx_num_);
#endif

  if (uart_event_queue_) {
    uart_driver_delete(uart_port_num_);
    uart_event_queue_ = nullptr;
  }
  if (byte_queue_) {
    delete byte_queue_;
    byte_queue_ = nullptr;
  }
}

ebus::Queue<ebus::BusEvent>* ebus::BusFreeRtos::getQueue() const {
  return byte_queue_;
}

void ebus::BusFreeRtos::writeByte(const uint8_t byte) {
  uart_write_bytes(uart_port_num_, static_cast<const void*>(&byte), 1);
}

void ebus::BusFreeRtos::setWindow(const uint16_t window) {
  portENTER_CRITICAL_ISR(&timer_mux_);
  window_ = window;
  portEXIT_CRITICAL_ISR(&timer_mux_);
}

void ebus::BusFreeRtos::setOffset(const uint16_t offset) {
  portENTER_CRITICAL_ISR(&timer_mux_);
  offset_ = offset;
  portEXIT_CRITICAL_ISR(&timer_mux_);
}

void ebus::BusFreeRtos::resetMetrics() {
#define X(name) counter_.name##_ = 0;
  EBUS_BUS_COUNTER_LIST
#undef X

#define X(name) name##_.reset();
  EBUS_BUS_TIMING_LIST
#undef X
}

ebus::metrics::BusMetrics ebus::BusFreeRtos::getMetrics() const {
  metrics::BusMetrics m;
  m.uptime = stats_uptime_.getValues();

  // Calculate Physical Utilization (%)
  double total_uptime = m.uptime.last;
  if (total_uptime > 0) {
    m.utilization = (stats_utilization_.getSum() / total_uptime) * 100.0;
  } else {
    m.utilization = 0.0;
  }

  return m;
}

void ebus::BusFreeRtos::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data. eBUS bit time is ~416.67us
  double low_time = (countZeroBits(byte) + 1) * (1000000.0 / 2400.0);
  stats_utilization_.addSample(low_time);
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
  uart_param_config(uart_port_num_, &uart_config);
  uart_set_pin(uart_port_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  // Install UART driver with event queue
  uart_driver_install(uart_port_num_, 256, 256, 1, &uart_event_queue_, 0);
  uart_set_rx_full_threshold(uart_port_num_, 1);
  uart_set_rx_timeout(uart_port_num_, 1);
}

void ebus::BusFreeRtos::configureGpio() {
  gpio_config_t gpio_conf = {
      .pin_bit_mask = (1ULL << rx_pin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };

  gpio_config(&gpio_conf);

  // Install GPIO ISR service with flags for IRAM and high priority (level 3)
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);

  // Register the ISR handler
  gpio_isr_handler_add(static_cast<gpio_num_t>(rx_pin_), &s_onFallingEdge,
                       this);
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

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config, &gp_timer_));

  gptimer_event_callbacks_t cbs = {
      .on_alarm = s_onBusIsrTimer,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gp_timer_, &cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(gp_timer_));

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
  timer_init(timer_group_num_, timer_idx_num_, &timer_config);
  timer_set_counter_value(timer_group_num_, timer_idx_num_, 0);

  // Register the ISR callback with flags for IRAM and high priority (level 3)
  timer_isr_callback_add(timer_group_num_, timer_idx_num_, s_onBusIsrTimer,
                         this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  timer_start(timer_group_num_, timer_idx_num_);
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
    if (xQueueReceive(uart_event_queue_, &event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(uart_port_num_, data, event.size, 0);
        for (int i = 0; i < len; ++i) {
          uint8_t byte = data[i];

          if (!byte_queue_) continue;

          if (byte == sym_syn && request_->busRequestPending()) {
            int64_t now = esp_timer_get_time();

            // Calculation of the expected start bit time based on the current
            // time and the bit time with a 0.5-bit offset. The expected start
            // bit time is calculated as follows:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - byte_time_;

            // Retrieving the start time of the last sync byte. Due to the
            // nature of the sync byte (0xaa), the buffer size used, and
            // hardware delays, the relevant index is two positions before the
            // current buffer index. This is because the sync byte is sent at
            // the beginning of the frame, and we want to align the start bit
            // with the sync byte. The buffer index is incremented in the
            // onFallingEdge ISR. Therefore, we need to access the position
            // bufferIndex + 2. This is the index of the last start bit.
            portENTER_CRITICAL_ISR(&timer_mux_);
            micros_start_bit_ = micros_edge_buffer_[(buffer_index_ + 2) %
                                                    FALLING_EDGE_BUFFER_SIZE];
            portEXIT_CRITICAL_ISR(&timer_mux_);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // start_bit_flag_ to true.
            int64_t delta =
                std::abs(expected_start_bit_time - micros_start_bit_);

            if (delta < static_cast<int64_t>(bit_time_ * 1.5f)) {
              int64_t micros_since_start_bit =
                  esp_timer_get_time() - micros_start_bit_;
              int64_t delay = window_ - micros_since_start_bit - offset_;
              if (delay < 0) delay = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
              gptimer_alarm_config_t alarm_config = {
                  .alarm_count = (uint64_t)delay,
                  .reload_count = 0,
                  .flags = {.auto_reload_on_alarm = false}};
              gptimer_stop(gp_timer_);
              gptimer_set_raw_count(gp_timer_, 0);
              gptimer_set_alarm_action(gp_timer_, &alarm_config);
              gptimer_start(gp_timer_);
#else
              // Legacy Timer Alarm setzen (v4.x)
              timer_set_counter_value(timer_group_num_, timer_idx_num_, 0);
              timer_set_alarm_value(timer_group_num_, timer_idx_num_,
                                    (double)delay);
              timer_set_alarm(timer_group_num_, timer_idx_num_, TIMER_ALARM_EN);
#endif

              portENTER_CRITICAL_ISR(&timer_mux_);
              micros_last_delay_ = delay;
              micros_delay_flag_ = true;
              portEXIT_CRITICAL_ISR(&timer_mux_);
            } else {
              portENTER_CRITICAL_ISR(&timer_mux_);
              start_bit_flag_ = true;
              portEXIT_CRITICAL_ISR(&timer_mux_);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          BusEvent bus_event;
          bus_event.byte = byte;

          portENTER_CRITICAL_ISR(&timer_mux_);
          bus_event.bus_request = bus_request_flag_;
          bus_event.start_bit = start_bit_flag_;
          if (micros_delay_flag_)
            stats_delay_.addSample(static_cast<double>(micros_last_delay_));
          if (micros_window_flag_)
            stats_window_.addSample(static_cast<double>(micros_last_window_));

          // Reset the global ISR flags after consumption
          bus_request_flag_ = false;
          start_bit_flag_ = false;
          portEXIT_CRITICAL_ISR(&timer_mux_);

          if (byte_queue_) byte_queue_->push(bus_event);
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
  portENTER_CRITICAL_ISR(&timer_mux_);
  buffer_index_ = (buffer_index_ + 1) % FALLING_EDGE_BUFFER_SIZE;
  micros_edge_buffer_[buffer_index_] = now;
  portEXIT_CRITICAL_ISR(&timer_mux_);
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
  uart_ll_write_txfifo(UART_LL_GET_HW(uart_port_num_), &byte, 1);
  portENTER_CRITICAL_ISR(&timer_mux_);
  micros_last_window_ = esp_timer_get_time() - micros_start_bit_;
  bus_request_flag_ = true;
  micros_window_flag_ = true;
  portEXIT_CRITICAL_ISR(&timer_mux_);
  return false;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
bool IRAM_ATTR ebus::BusFreeRtos::s_onSynGenTimer(
    gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata,
    void* user_ctx) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(user_ctx);
  return inst ? inst->onSynGenTimer() : false;
}
#else
bool IRAM_ATTR ebus::BusFreeRtos::s_onSynGenTimer(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  return inst ? inst->onSynGenTimer() : false;
}
#endif

bool ebus::BusFreeRtos::onSynGenTimer() {
  // Carrier Sense: Check if there was any activity very recently (e.g. within
  // 500us) that hasn't been processed by the task runner yet. if
  // (esp_timer_get_time() - lastActivityMicros_ < 500) {
  //   // Postpone slightly
  //   return true;
  // }

  syn_active_ = true;
  uint8_t syn = sym_syn;
  uart_ll_write_txfifo(UART_LL_GET_HW(uart_port_num_), &syn, 1);
  return false;
}

#endif
