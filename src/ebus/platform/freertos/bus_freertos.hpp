/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP32)
#include <atomic>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "core/bus_events.hpp"
#include "driver/uart.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/gptimer.h"
#else
#include "driver/timer.h"
#endif

namespace ebus::detail {

class Request;
class BusMonitor;

/**
 * FreeRtos-specific implementation of the eBUS physical layer.
 * Handles serial port configuration and asynchronous byte reading via a
 * background thread.
 */
class BusFreeRtos {
 public:
  using ReadListener = std::function<void(const uint8_t& byte)>;
  using WriteListener = std::function<void(const uint8_t& byte)>;
  using SynListener = std::function<void()>;

  explicit BusFreeRtos(const BusConfig& config, const RuntimeConfig& runtime,
                       Request* request, BusMonitor* monitor);
  ~BusFreeRtos();

  BusFreeRtos(const BusFreeRtos&) = delete;
  BusFreeRtos& operator=(const BusFreeRtos&) = delete;

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  void setWindow(const uint16_t window_us);
  void setOffset(const uint16_t offset_us);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  void recordUtilization(uint8_t byte);

  // Listeners
  void addReadListener(ReadListener listener);
  void addWriteListener(WriteListener listener);
  void addSynListener(SynListener listener);

 private:
  uart_port_t uart_port_num_;
  uint8_t rx_pin_;
  uint8_t tx_pin_;

  BusConfig config_;
  RuntimeConfig runtime_;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  gptimer_handle_t gp_timer_ = nullptr;
  gptimer_handle_t syn_gp_timer_ = nullptr;
  esp_timer_handle_t syn_postpone_timer_ = nullptr;
#else
  timer_group_t timer_group_num_ = TIMER_GROUP_1;
  timer_idx_t timer_idx_num_ = TIMER_0;
#endif

  Request* request_ = nullptr;
  BusMonitor* monitor_ = nullptr;

  // owned queue
  std::unique_ptr<Queue<BusEvent>> byte_queue_;

  std::unique_ptr<ServiceThread> worker_;
  std::atomic<bool> running_{false};

  std::vector<ReadListener> read_listeners_;
  std::vector<WriteListener> write_listeners_;
  std::vector<SynListener> syn_listeners_;

  // ISR/state
  static constexpr uint8_t FALLING_EDGE_BUFFER_SIZE = 5;

  // The byte time at 2400 baud for 10 bits with a 0.5-bit offset is
  // approximately 9.5 * bit_time_us = 9.5 * 416.67 us = 3958.33 us
  static constexpr int64_t byte_time_center_us_ =
      static_cast<int64_t>(Physical::byte_center_bits * Physical::bit_time_us);

  // This value can be adjusted if the bus ISR is not working as expected.
  volatile uint16_t window_us_ = 4300;  // usually between 4300-4456 us
  volatile uint16_t offset_us_ = 80;    // mainly for context switch and write

  volatile uint8_t buffer_index_ = 0;  // index for falling edge buffer
  volatile int64_t micros_edge_buffer_[FALLING_EDGE_BUFFER_SIZE] = {0};

  volatile int64_t micros_start_bit_ = 0;  // estimated start bit time

  volatile bool bus_request_flag_ = false;
  volatile bool start_bit_flag_ = false;

  volatile bool micros_delay_flag_ = false;
  volatile bool micros_window_flag_ = false;

  volatile int64_t micros_last_delay_ = 0;
  volatile int64_t micros_last_window_ = 0;

  volatile int64_t last_activity_micros_ = 0;
  // Time when we first wanted to send SYN
  volatile int64_t syn_intent_time_ = 0;

  std::atomic<bool> syn_running_{false};
  bool syn_active_{false};    // True if this generator is actively sending SYNs
  uint64_t syn_base_us_ = 0;  // Base SYN interval in microseconds
  uint64_t syn_unique_us_ = 0;  // Unique SYN interval in microseconds

  // platform handles
  QueueHandle_t uart_event_queue_ = nullptr;
  portMUX_TYPE timer_mux_ = portMUX_INITIALIZER_UNLOCKED;

  // setup helpers
  void configureUart();
  void configureGpio();
  void configureTimer();

  // UART event task: reads UART events and pushes bytes to byteQueue
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  static void s_onSynPostpone(void* arg);

  // ISR: Save the falling edges in order to estimate the sync byte
  static void IRAM_ATTR s_onFallingEdge(void* arg);
  void onFallingEdge();

  // ISR: Write request byte at the exact time
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static bool IRAM_ATTR s_onBusIsrTimer(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx);
  static bool IRAM_ATTR s_onSynGenTimer(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx);
#else
  static bool IRAM_ATTR s_onBusIsrTimer(void* arg);
  static bool IRAM_ATTR s_onSynGenTimer(void* arg);
#endif
  bool onBusIsrTimer();
  bool onSynGenTimer();
};

}  // namespace ebus::detail

#endif  // ESP32
