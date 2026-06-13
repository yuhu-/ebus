/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
#include <freertos/FreeRTOS.h>

#include <atomic>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/status.hpp>
#include <functional>
#include <memory>

#include "core/bus_events.hpp"
#include "driver/gptimer.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "platform/bus_base.hpp"
#include "platform/service_thread.hpp"

namespace ebus::detail {
class Request;
class BusMonitor;
}  // namespace ebus::detail

namespace ebus::detail::platform {

/**
 * ESP-specific implementation of the eBUS physical layer.
 * Handles serial port configuration and asynchronous byte reading via a
 * background thread.
 */
class BusEsp : public BusBase {
 public:
  // Public Types & Constants
  struct CriticalSection {
    portMUX_TYPE* mux;
    void lock() { portENTER_CRITICAL(mux); }
    void unlock() { portEXIT_CRITICAL(mux); }
  };

  struct CriticalSectionISR {
    portMUX_TYPE* mux;
    void lock() { portENTER_CRITICAL_ISR(mux); }
    void unlock() { portEXIT_CRITICAL_ISR(mux); }
  };

  // Lifecycle & Static Factories
  explicit BusEsp(const BusConfig& config, const RuntimeConfig& runtime,
                  detail::Request* request, detail::BusMonitor* monitor);
  ~BusEsp();
  void start();
  void stop();

  // Special Members & Operators
  BusEsp(const BusEsp&) = delete;
  BusEsp& operator=(const BusEsp&) = delete;

  // Configuration
  void setWindow(const uint16_t window_us);
  void setOffset(const uint16_t offset_us);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  // Working Methods
  void writeByte(const uint8_t byte);
  void recordUtilization(uint8_t byte);

  // Status/Telemetry
  platform::ServiceThread::Status getThreadStatus() const;
  platform::ServiceThread::Status getSynThreadStatus() const;
  ebus::BusStatus fetchStatus() const;

 private:
  BusConfig config_;
  RuntimeConfig runtime_;

  detail::Request* request_ = nullptr;
  detail::BusMonitor* monitor_ = nullptr;

  uart_port_t uart_port_num_;
  uint8_t rx_pin_;
  uint8_t tx_pin_;

  gptimer_handle_t gp_timer_ = nullptr;
  gptimer_handle_t syn_gp_timer_ = nullptr;
  esp_timer_handle_t syn_postpone_timer_ = nullptr;

  std::unique_ptr<ServiceThread> syn_worker_;

  std::unique_ptr<ServiceThread> worker_;
  std::atomic<bool> running_{false};

  // ISR/state
  static constexpr uint8_t falling_edge_buffer_size = 5;

  // The byte time at 2400 baud for 10 bits with a 0.5-bit offset is
  // approximately 9.5 * bit_time_us = 9.5 * 416.67 us = 3958.33 us
  static constexpr int64_t byte_time_center_us_ =
      static_cast<int64_t>(Physical::byte_center_bits * Physical::bit_time_us);

  uint8_t buffer_index_ = 0;  // Index for falling edge buffer
  int64_t micros_edge_buffer_[falling_edge_buffer_size] = {0};
  int64_t micros_start_bit_ = 0;  // Estimated start bit time

  bool bus_request_flag_ = false;
  bool start_bit_flag_ = false;
  bool micros_delay_flag_ = false;
  bool micros_window_flag_ = false;

  int64_t micros_last_delay_ = 0;
  int64_t micros_last_window_ = 0;

  int64_t last_activity_micros_ = 0;
  int64_t syn_intent_time_ = 0;
  int64_t syn_postpone_delta_us_ = 0;
  uint32_t syn_postponed_count_ = 0;
  bool syn_active_{false};

  std::atomic<bool> syn_running_{false};
  uint64_t syn_base_us_ = 0;    // Base SYN interval in microseconds
  uint64_t syn_unique_us_ = 0;  // Unique SYN interval in microseconds

  // platform handles
  QueueHandle_t uart_event_queue_ = nullptr;
  portMUX_TYPE timer_mux_ = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE listener_mux_ = portMUX_INITIALIZER_UNLOCKED;

  // setup helpers
  void configureUart();
  void configureGpio();
  void configureTimer();

  // UART event task: reads UART events and notifies listeners
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  static void s_onSynPostpone(void* arg);

  // ISR: Save the falling edges in order to estimate the sync byte
  static void IRAM_ATTR s_onFallingEdge(void* arg);
  void onFallingEdge();

  // ISR: Write request byte at the exact time
  static bool IRAM_ATTR s_onBusIsrTimer(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx);
  static bool IRAM_ATTR s_onSynGenTimer(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx);

  bool onBusIsrTimer();
  bool onSynGenTimer();
};

}  // namespace ebus::detail::platform

#endif  // ESP_PLATFORM
