/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP32)
#include <cstdint>
#include <ebus/config.hpp>
#include <functional>
#include <map>

#include "core/request.hpp"
#include "driver/uart.h"
#include "esp_idf_version.h"
#include "platform/queue.hpp"
#include "utils/timing_stats.hpp"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/gptimer.h"
#else
#include "driver/timer.h"
#endif

namespace ebus {

struct BusEvent {
  uint8_t byte;
  bool busRequest{false};
  bool startBit{false};
};

#define EBUS_BUS_COUNTER_LIST X(startBit)

#define EBUS_BUS_TIMING_LIST \
  X(stats_delay)             \
  X(stats_window)            \
  X(stats_transmit)          \
  X(stats_uptime)

class BusFreeRtos {
 public:
  explicit BusFreeRtos(const BusConfig& config, const RuntimeConfig& runtime,
                       Request* request);
  ~BusFreeRtos();

  BusFreeRtos(const BusFreeRtos&) = delete;
  BusFreeRtos& operator=(const BusFreeRtos&) = delete;

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  void resetMetrics();
  std::map<std::string, MetricValues> getMetrics() const;
  void recordUtilization(uint8_t byte);

 private:
  uart_port_t uart_port_num_;
  uint8_t rx_pin_;
  uint8_t tx_pin_;

  BusConfig config_;
  RuntimeConfig runtime_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  gptimer_handle_t gp_timer_ = nullptr;
  gptimer_handle_t syn_gp_timer_ = nullptr;
#else
  timer_group_t timerGroupNum_ = TIMER_GROUP_1;
  timer_idx_t timerIdxNum_ = TIMER_0;
#endif

  Request* request_ = nullptr;

  // owned queue
  Queue<BusEvent>* byte_queue_ = nullptr;

  // ISR/state
  static constexpr uint8_t FALLING_EDGE_BUFFER_SIZE = 5;

  // The bit time at 2400 baud is approximately 416.67 us
  static constexpr float bit_time_ = 1000000.0 / 2400.0;  // ~416.67 us

  // The byte time at 2400 baud for 10 bits with a 0.5-bit offset is
  // approximately 9.5 * bitTime_ = 9.5 * 416.67 us = 3958.33 us
  static constexpr int64_t byte_time_ = 9.5 * bit_time_;  // ~3958.33 us

  // This value can be adjusted if the bus ISR is not working as expected.
  volatile uint16_t window_ = 4300;  // usually between 4300-4456 us
  volatile uint16_t offset_ = 80;    // mainly for context switch and write

  volatile uint8_t buffer_index_ = 0;  // index for falling edge buffer
  volatile int64_t micros_edge_buffer_[FALLING_EDGE_BUFFER_SIZE] = {0};

  volatile int64_t micros_start_bit_ = 0;  // estimated start bit time

  volatile bool bus_request_flag_ = false;
  volatile bool start_bit_flag_ = false;

  volatile bool micros_delay_flag_ = false;
  volatile bool micros_window_flag_ = false;

  volatile int64_t micros_last_delay_ = 0;
  volatile int64_t micros_last_window_ = 0;

  std::atomic<bool> syn_running_{false};
  bool syn_active_{false};
  uint64_t syn_base_us_ = 0;
  uint64_t syn_unique_us_ = 0;

  // platform handles
  QueueHandle_t uart_event_queue_ = nullptr;
  portMUX_TYPE timer_mux_ = portMUX_INITIALIZER_UNLOCKED;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_BUS_COUNTER_LIST
#undef X
  };

  Counter counter_;

  TimingStats stats_delay_;
  TimingStats stats_window_;
  TimingStats stats_transmit_;
  RollingStats stats_utilization_;
  TimingStats stats_uptime_;

  // setup helpers
  void configureUart();
  void configureGpio();
  void configureTimer();

  // UART event task: reads UART events and pushes bytes to byteQueue
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  // SYN generation logic
  void checkSynGenerator();

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

}  // namespace ebus

#endif
