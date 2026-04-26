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

#include <ebus/protocol_math.hpp>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "driver/gpio.h"

namespace ebus::detail {

ebus::BusFreeRtos::BusFreeRtos(const BusConfig& config,
                               const RuntimeConfig& runtime, Request* request,
                               BusMonitor* monitor)
    : uart_port_num_(static_cast<uart_port_t>(config.uart_port)),
      rx_pin_(config.rx_pin),
      tx_pin_(config.tx_pin),
      runtime_(runtime),
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
      timer_group_num_(static_cast<timer_group_t>(config.timer_group)),
      timer_idx_num_(static_cast<timer_idx_t>(config.timer_idx)),
#endif
      request_(request),
      monitor_(monitor),
      byte_queue_(std::make_unique<ebus::Queue<ebus::BusEvent>>(
          detail::Bus::queue_size)) {

  // Initialize postponement timer
  const esp_timer_create_args_t postpone_args = {
      .callback = &BusFreeRtos::s_onSynPostpone,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "ebusSynPostpone",
      .skip_unhandled_events = true};
  ESP_ERROR_CHECK(esp_timer_create(&postpone_args, &syn_postpone_timer_));

  configureUart();
  configureGpio();
  configureTimer();

  start();
}

ebus::BusFreeRtos::~BusFreeRtos() { stop(); }

void ebus::BusFreeRtos::start() {
  xTaskCreate(&BusFreeRtos::s_ebusUartEventRunner, "ebusUartEventRunner", 2048,
              this, configMAX_PRIORITIES - 1, nullptr);
}

void ebus::BusFreeRtos::stop() {
  gpio_isr_handler_remove(static_cast<gpio_num_t>(rx_pin_));
  gpio_intr_disable(static_cast<gpio_num_t>(rx_pin_));
  gpio_set_intr_type(static_cast<gpio_num_t>(rx_pin_), GPIO_INTR_DISABLE);

  if (syn_postpone_timer_) {
    esp_timer_stop(syn_postpone_timer_);
    esp_timer_delete(syn_postpone_timer_);
    syn_postpone_timer_ = nullptr;
  }

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
}

ebus::Queue<ebus::BusEvent>* ebus::BusFreeRtos::getQueue() const {
  return byte_queue_.get();
}

void ebus::BusFreeRtos::writeByte(const uint8_t byte) {
  for (const auto& listener : write_listeners_) listener(byte);
  if (monitor_) monitor_->transmit.markBegin();
  portENTER_CRITICAL_ISR(&timer_mux_);
  last_activity_micros_ = esp_timer_get_time();
  portEXIT_CRITICAL_ISR(&timer_mux_);
  uart_write_bytes(uart_port_num_, static_cast<const void*>(&byte), 1);
  if (monitor_) monitor_->transmit.markEnd();
}

void ebus::BusFreeRtos::setWindow(const uint16_t window) {
  portENTER_CRITICAL_ISR(&timer_mux_);
  // Validate window against limits
  window_ =
      (window < detail::Bus::window_min || window > detail::Bus::window_max)
          ? ebus::RuntimeConfig{}.bus.window_us
          : window;
  portEXIT_CRITICAL_ISR(&timer_mux_);
}

void ebus::BusFreeRtos::setOffset(const uint16_t offset) {
  portENTER_CRITICAL_ISR(&timer_mux_);
  // Validate offset against limits
  offset_ = (offset > detail::Bus::offset_max)
                ? ebus::RuntimeConfig{}.bus.offset_us
                : offset;
  portEXIT_CRITICAL_ISR(&timer_mux_);
}

void ebus::BusFreeRtos::setRuntimeConfig(const RuntimeConfig& runtime) {
  bool was_enabled = runtime_.bus.syn.enabled;
  runtime_ = runtime;

  // Validate window and offset
  if (runtime_.bus.window_us < detail::Bus::window_min ||
      runtime_.bus.window_us > detail::Bus::window_max)
    runtime_.bus.window_us = ebus::RuntimeConfig{}.bus.window_us;
  if (runtime_.bus.offset_us > detail::Bus::offset_max)
    runtime_.bus.offset_us = ebus::RuntimeConfig{}.bus.offset_us;

  // Calculate microsecond timings for hardware timers
  syn_base_us_ = static_cast<uint64_t>(runtime_.bus.syn.base_ms) * 1000;
  syn_unique_us_ =
      syn_base_us_ +
      (static_cast<uint64_t>(runtime_.address) *
       static_cast<uint64_t>(detail::Bus::Syn::address_factor_ms) * 1000) +
      (static_cast<uint64_t>(runtime_.bus.syn.tolerance_ms) * 1000);

  if (runtime_.bus.syn.enabled) {
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = syn_unique_us_,  // Start with unique rate
        .reload_count = 0,
        .flags = {.auto_reload_on_alarm = true}};

    gptimer_set_alarm_action(syn_gp_timer_, &alarm_config);

    if (!was_enabled) {
      syn_running_.store(true);
      syn_active_ = false;
      gptimer_start(syn_gp_timer_);
    }
  } else if (was_enabled) {
    syn_running_.store(false);
    gptimer_stop(syn_gp_timer_);
  }
}

void ebus::BusFreeRtos::addReadListener(ReadListener listener) {
  read_listeners_.push_back(std::move(listener));
}

void ebus::BusFreeRtos::addWriteListener(WriteListener listener) {
  write_listeners_.push_back(std::move(listener));
}

void ebus::BusFreeRtos::addSynListener(SynListener listener) {
  syn_listeners_.push_back(std::move(listener));
}

void ebus::BusFreeRtos::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data. eBUS bit time is ~416.67us
  double low_time = (countZeroBits(byte) + 1) * ebus::Physical::bit_time_us;
  if (monitor_) monitor_->utilization.addSample(low_time);
}

void ebus::BusFreeRtos::configureUart() {
  uart_config_t uart_config = {
      .baud_rate = ebus::Physical::baud_rate,
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
  gptimer_config_t gpt_config_arb = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,  // 1 Tick = 1 µs
      .intr_priority = 3,        //  high priority (1-3)
      .flags = {.intr_shared = false,
                .allow_pd = false,
                .backup_before_sleep = false}};

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config_arb, &gp_timer_));

  gptimer_config_t gpt_config_syn = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
      .intr_priority = 3,
      .flags = {
          .intr_shared = false,  // do not share interrupt - reduce jitter
          .allow_pd = false,
          .backup_before_sleep = false,
      }};

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config_syn, &syn_gp_timer_));

  gptimer_event_callbacks_t arb_cbs = {
      .on_alarm = s_onBusIsrTimer,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gp_timer_, &arb_cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(gp_timer_));

  gptimer_event_callbacks_t syn_cbs = {
      .on_alarm = s_onSynGenTimer,
  };
  ESP_ERROR_CHECK(
      gptimer_register_event_callbacks(syn_gp_timer_, &syn_cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(syn_gp_timer_));

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
  uart_event_t uart_event;
  uint8_t data[128];
  for (;;) {
    if (xQueueReceive(uart_event_queue_, &uart_event, portMAX_DELAY)) {
      if (uart_event.type == UART_DATA) {
        int len = uart_read_bytes(uart_port_num_, data, uart_event.size, 0);
        for (int i = 0; i < len; ++i) {
          auto arrival_time = std::chrono::steady_clock::now();
          uint8_t byte = data[i];

          for (const auto& listener : read_listeners_) listener(byte);

          recordUtilization(byte);

          if (!byte_queue_) continue;

          if (byte == Symbols::syn && request_->busRequestPending()) {
            int64_t now = esp_timer_get_time();  // Current time in microseconds

            // Calculation of the expected start bit time based on the current
            // time and the bit time with a 0.5-bit offset. The expected start
            // bit time is calculated as follows:
            // now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67
            int64_t expected_start_bit_time = now - byte_time_center_us_;

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

            if (delta <  // within 1.5 bit times
                static_cast<int64_t>(ebus::Physical::bit_time_us * 1.5f)) {
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
              if (monitor_)
                monitor_->updateBus([](auto& m) { m.start_bit_errors++; });
              portEXIT_CRITICAL_ISR(&timer_mux_);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          BusEvent bus_event;
          bus_event.byte = byte;

          portENTER_CRITICAL_ISR(&timer_mux_);
          bus_event.bus_request = bus_request_flag_;
          bus_event.start_bit = start_bit_flag_;

          if (monitor_) {
            if (micros_delay_flag_)
              monitor_->delay.addSample(
                  static_cast<double>(micros_last_delay_));
            if (micros_window_flag_)
              monitor_->window.addSample(
                  static_cast<double>(micros_last_window_));
          }

          // Reset the global ISR flags after consumption
          bus_request_flag_ = false;
          start_bit_flag_ = false;
          micros_delay_flag_ = false;
          micros_window_flag_ = false;
          portEXIT_CRITICAL_ISR(&timer_mux_);

          bus_event.timestamp = arrival_time;

          if (byte_queue_) byte_queue_->push(bus_event);

          // Reset SYN Timer (Arbitration Logic)
          if (runtime_.bus.syn.enabled) {
            uint64_t next_interval;
            if (syn_active_ && byte == Symbols::syn) {
              // We won! Continue at fast rate
              next_interval = syn_base_us_;
            } else {
              // Someone else is master, or we collided. Switch to slow rate.
              syn_active_ = false;
              next_interval = syn_unique_us_;
            }

            // Update the hardware timer alarm
            gptimer_alarm_config_t alarm_config = {
                .alarm_count = next_interval,
                .reload_count = 0,
                .flags = {.auto_reload_on_alarm = true}};
            gptimer_stop(syn_gp_timer_);  // Ensure thread-safe reconfiguration
            gptimer_set_raw_count(syn_gp_timer_, 0);  // Restart count
            gptimer_set_alarm_action(syn_gp_timer_, &alarm_config);
            gptimer_start(syn_gp_timer_);
          }
        }
      }
    }
  }
}

void ebus::BusFreeRtos::s_onSynPostpone(void* arg) {
  BusFreeRtos* inst = static_cast<BusFreeRtos*>(arg);
  if (inst) inst->onSynGenTimer();
}

// static ISR trampoline -> instance method
void IRAM_ATTR ebus::BusFreeRtos::s_onFallingEdge(void* arg) {
  BusFreeRtos* inst = reinterpret_cast<BusFreeRtos*>(arg);
  if (inst) inst->onFallingEdge();
}

void IRAM_ATTR ebus::BusFreeRtos::onFallingEdge() {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timer_mux_);
  buffer_index_ = (buffer_index_ + 1) % FALLING_EDGE_BUFFER_SIZE;
  last_activity_micros_ = now;
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

bool IRAM_ATTR ebus::BusFreeRtos::onBusIsrTimer() {
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

bool IRAM_ATTR ebus::BusFreeRtos::onSynGenTimer() {
  int64_t now = esp_timer_get_time();

  portENTER_CRITICAL_ISR(&timer_mux_);
  int64_t last_activity = last_activity_micros_;
  portEXIT_CRITICAL_ISR(&timer_mux_);

  // Carrier Sense: yield and postpone if bus was active within the last 5ms
  // (Duration of a byte at 2400 baud + safety margin)
  if (now - last_activity < (detail::Bus::Syn::carrier_sense_ms * 1000)) {
    if (syn_intent_time_ == 0) syn_intent_time_ = now;
    // Schedule a re-check in 2ms using the ISR-safe esp_timer
    if (monitor_) {
      monitor_->updateBus([](auto& m) { m.syn_postponed_count++; });
    }
    esp_timer_start_once(syn_postpone_timer_,
                         detail::Bus::Syn::postpone_ms * 1000);
    return false;
  }

  if (syn_intent_time_ > 0 && monitor_) {
    monitor_->syn_postpone.addSample(
        static_cast<double>(now - syn_intent_time_));
    syn_intent_time_ = 0;
  }

  syn_active_ = true;  // Mark SYN generator as active
  portENTER_CRITICAL_ISR(&timer_mux_);
  last_activity_micros_ = now;
  portEXIT_CRITICAL_ISR(&timer_mux_);

  for (const auto& listener : syn_listeners_) listener();

  uint8_t syn = Symbols::syn;
  uart_ll_write_txfifo(UART_LL_GET_HW(uart_port_num_), &syn, 1);
  return false;
}

}  // namespace ebus::detail
#endif  // ESP32
