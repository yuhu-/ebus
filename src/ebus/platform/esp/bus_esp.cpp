/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
#include "platform/esp/bus_esp.hpp"

#include <esp_timer.h>
#include <hal/uart_ll.h>

#include <cmath>
#include <ebus/protocol_math.hpp>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "driver/gpio.h"
#include "esp_clk_tree.h"
#include "platform/system.hpp"
#include "utils/logger.hpp"

namespace ebus::detail::platform {

BusEsp::BusEsp(const BusConfig& config, const RuntimeConfig& runtime,
               Request* request, BusMonitor* monitor)
    : config_(config),
      runtime_(runtime),
      request_(request),
      monitor_(monitor),
      uart_port_num_(static_cast<uart_port_t>(config.uart_port)),
      rx_pin_(config.rx_pin),
      tx_pin_(config.tx_pin) {
  // Initialize postponement timer
  esp_timer_create_args_t postpone_args = {};
  postpone_args.callback = &BusEsp::s_onSynPostpone;
  postpone_args.arg = this;
  postpone_args.dispatch_method = ESP_TIMER_TASK;
  postpone_args.name = "ebusSynPostpone";
  postpone_args.skip_unhandled_events = true;
  ESP_ERROR_CHECK(esp_timer_create(&postpone_args, &syn_postpone_timer_));

  configureUart();
  configureGpio();
  configureTimer();
}

BusEsp::~BusEsp() { stop(); }

void BusEsp::start() {
  if (running_.load(std::memory_order_acquire)) return;

  if (!uart_event_queue_) {
    EBUS_LOG_ERROR(
        "Cannot start ebus_bus: UART driver not installed (queue null)");
    return;
  }

  running_.store(true, std::memory_order_release);
  worker_ = std::make_unique<ServiceThread>(
      "ebus_bus", [this] { ebusUartEventRunner(); },
      OrchestrationLimits::bus_stack_size, OrchestrationLimits::bus_priority);
  worker_->start();
}

void BusEsp::stop() {
  if (!running_.load(std::memory_order_acquire)) return;
  running_.store(false, std::memory_order_release);
  syn_running_.store(false);

  gpio_isr_handler_remove(static_cast<gpio_num_t>(rx_pin_));
  gpio_intr_disable(static_cast<gpio_num_t>(rx_pin_));
  gpio_set_intr_type(static_cast<gpio_num_t>(rx_pin_), GPIO_INTR_DISABLE);

  if (syn_postpone_timer_) {
    esp_timer_stop(syn_postpone_timer_);
    esp_timer_delete(syn_postpone_timer_);
    syn_postpone_timer_ = nullptr;
  }

  if (gp_timer_) {
    gptimer_stop(gp_timer_);
    gptimer_disable(gp_timer_);
    gptimer_del_timer(gp_timer_);
    gp_timer_ = nullptr;
  }

  if (syn_worker_) syn_worker_->join();
  if (worker_) worker_->join();

  if (uart_event_queue_) {
    uart_driver_delete(uart_port_num_);
    uart_event_queue_ = nullptr;
  }
}

void BusEsp::setWindow(const uint16_t window_us) {
  portENTER_CRITICAL(&timer_mux_);
  // Validate window against limits
  runtime_.bus.window_us = (window_us < BusLimits::window_min_us ||
                            window_us > BusLimits::window_max_us)
                               ? ebus::RuntimeConfig{}.bus.window_us
                               : window_us;
  portEXIT_CRITICAL(&timer_mux_);
}

void BusEsp::setOffset(const uint16_t offset_us) {
  portENTER_CRITICAL(&timer_mux_);
  // Validate offset against limits
  runtime_.bus.offset_us = (offset_us > BusLimits::offset_max_us)
                               ? ebus::RuntimeConfig{}.bus.offset_us
                               : offset_us;
  portEXIT_CRITICAL(&timer_mux_);
}

void BusEsp::setRuntimeConfig(const RuntimeConfig& runtime) {
  bool was_enabled;
  uint64_t base_us = static_cast<uint64_t>(BusLimits::Syn::base_ms) * 1000;
  uint64_t unique_us =
      base_us +  //
      (static_cast<uint64_t>(runtime.address) *
       static_cast<uint64_t>(BusLimits::Syn::address_factor_ms) * 1000) +
      (static_cast<uint64_t>(BusLimits::Syn::tolerance_ms) * 1000);

  portENTER_CRITICAL(&timer_mux_);
  was_enabled = runtime_.bus.syn_gen;
  runtime_ = runtime;

  // Validate window and offset inside the critical section
  if (runtime_.bus.window_us < BusLimits::window_min_us ||
      runtime_.bus.window_us > BusLimits::window_max_us)
    runtime_.bus.window_us = ebus::RuntimeConfig{}.bus.window_us;
  if (runtime_.bus.offset_us > BusLimits::offset_max_us)
    runtime_.bus.offset_us = ebus::RuntimeConfig{}.bus.offset_us;

  syn_base_us_ = base_us;
  syn_unique_us_ = unique_us;
  portEXIT_CRITICAL(&timer_mux_);

  if (runtime_.bus.syn_gen) {
    gptimer_alarm_config_t alarm_config = {};
    alarm_config.alarm_count = syn_unique_us_;
    alarm_config.reload_count = 0;
    alarm_config.flags.auto_reload_on_alarm = true;

    if (!was_enabled) {
      syn_running_.store(true);
      syn_active_ = false;
      gptimer_set_alarm_action(syn_gp_timer_, &alarm_config);
      gptimer_start(syn_gp_timer_);
    } else {
      // Restart timer to apply the new t_unique count immediately
      gptimer_stop(syn_gp_timer_);
      gptimer_set_raw_count(syn_gp_timer_, 0);
      gptimer_set_alarm_action(syn_gp_timer_, &alarm_config);
      gptimer_start(syn_gp_timer_);
    }
  } else if (was_enabled) {
    syn_running_.store(false);
    gptimer_stop(syn_gp_timer_);
  }
}

void BusEsp::writeByte(const uint8_t byte) {
  lockAndInvoke(listeners_mutex_, getWriteListeners(), byte);

  if (monitor_) monitor_->transmit.markBegin();

  portENTER_CRITICAL(&timer_mux_);
  last_activity_micros_ = esp_timer_get_time();
  portEXIT_CRITICAL(&timer_mux_);

  // Use a 10ms timeout to avoid blocking the high-priority bus task
  uart_write_bytes(uart_port_num_, static_cast<const void*>(&byte), 1,
                   pdMS_TO_TICKS(10));

  if (monitor_) monitor_->transmit.markEnd();
}

void BusEsp::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data.
  if (monitor_) monitor_->recordLowBits(countZeroBits(byte) + 1);
}

ServiceThread::Status BusEsp::getThreadStatus() const {
  if (worker_) {
    return worker_->status();
  }
  return ServiceThread::Status{"ebus_bus", -1, -1};
}

ServiceThread::Status BusEsp::getSynThreadStatus() const {
  if (syn_worker_) {
    return syn_worker_->status();
  }
  return ServiceThread::Status{"ebus_bus_syn", -1, -1};
}

ebus::BusStatus BusEsp::getStatus() const {
  auto map =
      [](const platform::ServiceThread::Status& s) -> ebus::ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };
  return {map(getThreadStatus()), map(getSynThreadStatus())};
}

void BusEsp::configureUart() {
  uart_config_t uart_config = {};
  uart_config.baud_rate = Physical::baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 0;
  uart_config.source_clk = UART_SCLK_APB;
  uart_config.flags.allow_pd = false;
  uart_config.flags.backup_before_sleep = false;

  // Install UART driver with synchronized event queue sizes
  uart_driver_delete(uart_port_num_);  // Ensure port is clean
  esp_err_t err = ESP_FAIL;
  int retry_count = 3;
  while (retry_count-- > 0) {
    err = uart_driver_install(uart_port_num_, BusLimits::queue_size,
                              BusLimits::queue_size, 1, &uart_event_queue_, 0);
    if (err == ESP_OK) break;

    EBUS_LOG_ERROR("uart_driver_install attempt failed: " +
                   std::string(esp_err_to_name(err)) + ". Retrying...");
    if (retry_count > 0) vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Setup UART configuration AFTER driver is installed (recommended)
  uart_param_config(uart_port_num_, &uart_config);

  if (err != ESP_OK) {
    EBUS_LOG_ERROR("uart_driver_install failed after all retries: " +
                   std::string(esp_err_to_name(err)));
    return;
  }

  uart_set_pin(uart_port_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);
  uart_set_rx_full_threshold(uart_port_num_, 1);
  uart_set_rx_timeout(uart_port_num_, 1);
}

void BusEsp::configureGpio() {
  gpio_config_t gpio_conf = {};
  gpio_conf.pin_bit_mask = (1ULL << rx_pin_);
  gpio_conf.mode = GPIO_MODE_INPUT;
  gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_conf.intr_type = GPIO_INTR_NEGEDGE;

  esp_err_t err = gpio_config(&gpio_conf);
  if (err != ESP_OK) {
    EBUS_LOG_ERROR("GPIO config for RX pin failed: " +
                   std::string(esp_err_to_name(err)));
  }

  // Install GPIO ISR service with flags for IRAM and high priority (level 3)
  err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    EBUS_LOG_ERROR("gpio_install_isr_service failed: " +
                   std::string(esp_err_to_name(err)));
  }

  // Register the ISR handler
  err = gpio_isr_handler_add(static_cast<gpio_num_t>(rx_pin_), &s_onFallingEdge,
                             this);
  if (err != ESP_OK) {
    EBUS_LOG_ERROR("gpio_isr_handler_add failed: " +
                   std::string(esp_err_to_name(err)));
  }
}

void BusEsp::configureTimer() {
  gptimer_config_t gpt_config_arb = {};
  gpt_config_arb.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  gpt_config_arb.direction = GPTIMER_COUNT_UP;
  gpt_config_arb.resolution_hz = 1000000;
  gpt_config_arb.intr_priority = 3;
  gpt_config_arb.flags.intr_shared = false;
  gpt_config_arb.flags.allow_pd = false;
  gpt_config_arb.flags.backup_before_sleep = false;

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config_arb, &gp_timer_));

  gptimer_config_t gpt_config_syn = {};
  gpt_config_syn.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  gpt_config_syn.direction = GPTIMER_COUNT_UP;
  gpt_config_syn.resolution_hz = 1000000;
  gpt_config_syn.intr_priority = 3;
  gpt_config_syn.flags.intr_shared = false;
  gpt_config_syn.flags.allow_pd = false;
  gpt_config_syn.flags.backup_before_sleep = false;

  ESP_ERROR_CHECK(gptimer_new_timer(&gpt_config_syn, &syn_gp_timer_));

  gptimer_event_callbacks_t arb_cbs = {};
  arb_cbs.on_alarm = s_onBusIsrTimer;
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gp_timer_, &arb_cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(gp_timer_));

  gptimer_event_callbacks_t syn_cbs = {};
  syn_cbs.on_alarm = s_onSynGenTimer;
  ESP_ERROR_CHECK(
      gptimer_register_event_callbacks(syn_gp_timer_, &syn_cbs, this));
  ESP_ERROR_CHECK(gptimer_enable(syn_gp_timer_));
}

// static trampoline for FreeRTOS task
void BusEsp::s_ebusUartEventRunner(void* arg) {
  BusEsp* self = static_cast<BusEsp*>(arg);
  if (self) self->ebusUartEventRunner();
}

void BusEsp::ebusUartEventRunner() {
  uart_event_t uart_event;
  uint8_t data[SequenceLimits::default_capacity * 2];

  while (running_.load(std::memory_order_acquire)) {
    if (!uart_event_queue_) {
      break;
    }

    if (xQueueReceive(
            uart_event_queue_, &uart_event,
            pdMS_TO_TICKS(BusLimits::platform::Esp::event_timeout_ms))) {
      if (uart_event.type == UART_DATA) {
        const int len =
            uart_read_bytes(uart_port_num_, data, uart_event.size, 0);
        for (int i = 0; i < len; ++i) {
          const auto arrival_time = Clock::now();
          const uint8_t byte = data[i];

          CriticalSection l_lock{&listener_mux_};
          lockAndInvoke(l_lock, getReadListeners(), byte);

          recordUtilization(byte);

          if (byte == Symbols::syn && request_->busRequestPending()) {
            const int64_t now =
                esp_timer_get_time();  // Current time in microseconds

            /* Calculation of the expected start bit time based on the current
               time and the bit time with a 0.5-bit offset. The expected start
               bit time is calculated as follows:
               now - (10 * 416.67) + (0.5 * 416.67) or: now - 9.5 * 416.67 */
            const int64_t expected_start_bit_time = now - byte_time_center_us_;

            // Retrieving the start time of the last sync byte. Due to the
            // nature of the sync byte (0xAA), the buffer size used, and
            // hardware delays, the relevant index is two positions before the
            // current buffer index. This is because the sync byte is sent at
            // the beginning of the frame, and we want to align the start bit
            // with the sync byte. The buffer index is incremented in the
            // onFallingEdge ISR. Therefore, we need to access the position
            // bufferIndex + 2. This is the index of the last start bit.
            portENTER_CRITICAL(&timer_mux_);
            micros_start_bit_ = micros_edge_buffer_[(buffer_index_ + 2) %
                                                    FALLING_EDGE_BUFFER_SIZE];
            const uint16_t window = runtime_.bus.window_us;
            const uint16_t offset = runtime_.bus.offset_us;
            portEXIT_CRITICAL(&timer_mux_);

            // Calculate the difference between the expected start bit time
            // and the actual start bit time. If the difference is within 1.5
            // bit times, we consider it a valid start bit. This is to account
            // for slight variations in timing due to processing delays or
            // other factors. If the difference is larger than 1.5 bit times, we
            // consider it an unexpected start bit, and we set the
            // start_bit_flag_ to true.
            const int64_t delta =
                std::abs(expected_start_bit_time - micros_start_bit_);

            if (delta < static_cast<int64_t>(Physical::bit_time_us *
                                             Physical::start_bit_tolerance)) {
              const int64_t micros_since_start_bit =
                  esp_timer_get_time() - micros_start_bit_;
              const int64_t delay =
                  (window > micros_since_start_bit + offset)
                      ? (window - micros_since_start_bit - offset)
                      : 0;

              gptimer_alarm_config_t alarm_config = {};
              alarm_config.alarm_count = (uint64_t)delay;
              alarm_config.reload_count = 0;
              alarm_config.flags.auto_reload_on_alarm = false;
              gptimer_stop(gp_timer_);
              gptimer_set_raw_count(gp_timer_, 0);
              gptimer_set_alarm_action(gp_timer_, &alarm_config);
              gptimer_start(gp_timer_);

              portENTER_CRITICAL(&timer_mux_);
              micros_last_delay_ = delay;
              micros_delay_flag_ = true;
              portEXIT_CRITICAL(&timer_mux_);
            } else {
              portENTER_CRITICAL(&timer_mux_);
              start_bit_flag_ = true;
              if (monitor_) monitor_->recordIsrStartBitError();
              portEXIT_CRITICAL(&timer_mux_);
            }
          }

          // capture ISR flags and timing atomically and clear globals
          BusEvent bus_event;
          bus_event.byte = byte;

          portENTER_CRITICAL(&timer_mux_);
          bus_event.bus_request = bus_request_flag_;
          bus_event.start_bit = start_bit_flag_;
          const bool has_delay = micros_delay_flag_;
          const bool has_window = micros_window_flag_;
          const int64_t last_delay = micros_last_delay_;
          const int64_t last_window = micros_last_window_;

          int64_t postpone_sample = 0;
          if (syn_postpone_delta_us_ > 0) {
            postpone_sample = syn_postpone_delta_us_;
            syn_postpone_delta_us_ = 0;
          }
          uint32_t postponed_count = 0;
          if (syn_postponed_count_ > 0) {
            postponed_count = syn_postponed_count_;
            syn_postponed_count_ = 0;
          }

          // Reset the global ISR flags after consumption
          bus_request_flag_ = false;
          start_bit_flag_ = false;
          micros_delay_flag_ = false;
          micros_window_flag_ = false;
          portEXIT_CRITICAL(&timer_mux_);

          if (monitor_) {
            if (has_delay)
              monitor_->delay.addSample(static_cast<float>(last_delay));
            if (has_window)
              monitor_->window.addSample(static_cast<float>(last_window));
            if (postpone_sample > 0) {
              monitor_->syn_postpone.addSample(
                  static_cast<float>(postpone_sample));
            }
            if (postponed_count > 0) {
              monitor_->recordIsrSynPostponed(postponed_count);
            }
          }

          bus_event.timestamp = arrival_time;

          CriticalSection l_lock{&listener_mux_};
          lockAndInvoke(l_lock, getBusEventListeners(), bus_event);

          // Reset SYN Timer (Arbitration Logic)
          portENTER_CRITICAL(&timer_mux_);
          const bool syn_enabled = runtime_.bus.syn_gen;
          const uint64_t base_us = syn_base_us_;
          const uint64_t unique_us = syn_unique_us_;
          portEXIT_CRITICAL(&timer_mux_);

          if (syn_enabled) {
            uint64_t next_interval;
            portENTER_CRITICAL(&timer_mux_);
            if (syn_active_ && byte == Symbols::syn) {  // Check echo for 0xAA
              next_interval = base_us;
            } else {
              syn_active_ = false;
              next_interval = unique_us;
            }
            portEXIT_CRITICAL(&timer_mux_);

            // Update the hardware timer alarm
            gptimer_alarm_config_t alarm_config = {};
            alarm_config.alarm_count = next_interval;
            alarm_config.reload_count = 0;
            alarm_config.flags.auto_reload_on_alarm = true;
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

void BusEsp::s_onSynPostpone(void* arg) {
  BusEsp* inst = static_cast<BusEsp*>(arg);
  if (inst) inst->onSynGenTimer();
}

// static ISR trampoline -> instance method
void IRAM_ATTR BusEsp::s_onFallingEdge(void* arg) {
  BusEsp* inst = reinterpret_cast<BusEsp*>(arg);
  if (inst) inst->onFallingEdge();
}

void IRAM_ATTR BusEsp::onFallingEdge() {
  int64_t now = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&timer_mux_);
  buffer_index_ =
      (buffer_index_ + 1) % BusLimits::platform::Esp::falling_edge_history;
  last_activity_micros_ = now;
  micros_edge_buffer_[buffer_index_] = now;
  portEXIT_CRITICAL_ISR(&timer_mux_);
}

// static ISR trampoline -> instance method
bool IRAM_ATTR BusEsp::s_onBusIsrTimer(gptimer_handle_t timer,
                                       const gptimer_alarm_event_data_t* edata,
                                       void* user_ctx) {
  BusEsp* inst = reinterpret_cast<BusEsp*>(user_ctx);
  return inst ? inst->onBusIsrTimer() : false;
}

bool IRAM_ATTR BusEsp::onBusIsrTimer() {
  uint8_t byte = request_->busRequestAddress();
  uart_ll_write_txfifo(UART_LL_GET_HW(uart_port_num_), &byte, 1);
  portENTER_CRITICAL_ISR(&timer_mux_);
  micros_last_window_ = esp_timer_get_time() - micros_start_bit_;
  bus_request_flag_ = true;
  micros_window_flag_ = true;
  portEXIT_CRITICAL_ISR(&timer_mux_);
  return false;
}

bool IRAM_ATTR BusEsp::s_onSynGenTimer(gptimer_handle_t timer,
                                       const gptimer_alarm_event_data_t* edata,
                                       void* user_ctx) {
  BusEsp* inst = reinterpret_cast<BusEsp*>(user_ctx);
  return inst ? inst->onSynGenTimer() : false;
}

bool IRAM_ATTR BusEsp::onSynGenTimer() {
  int64_t now = esp_timer_get_time();

  portENTER_CRITICAL_ISR(&timer_mux_);
  int64_t last_activity = last_activity_micros_;

  /* Carrier Sense: yield and postpone if bus was active within the last 5ms
     (Duration of a byte at 2400 baud + safety margin) */
  if (now - last_activity < (BusLimits::Syn::carrier_sense_ms * 1000)) {
    if (syn_intent_time_ == 0) syn_intent_time_ = now;
    syn_postponed_count_++;
    portEXIT_CRITICAL_ISR(&timer_mux_);

    // Schedule a re-check in 2ms using the ISR-safe esp_timer
    esp_timer_start_once(syn_postpone_timer_,
                         BusLimits::Syn::postpone_ms * 1000);
    return false;
  }

  if (syn_intent_time_ > 0) {
    syn_postpone_delta_us_ = now - syn_intent_time_;
    syn_intent_time_ = 0;
  }

  syn_active_ = true;
  last_activity_micros_ = now;
  portEXIT_CRITICAL_ISR(&timer_mux_);

  CriticalSectionISR l_lock_isr{&listener_mux_};
  lockAndInvoke(l_lock_isr, getSynListeners());

  uint8_t syn = Symbols::syn;
  uart_ll_write_txfifo(UART_LL_GET_HW(uart_port_num_), &syn, 1);
  return false;
}

}  // namespace ebus::detail::platform

#endif  // ESP_PLATFORM
