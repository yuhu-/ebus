/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/status.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "core/bus_events.hpp"
#include "driver/gptimer.h"
#include "driver/uart.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

#if EBUS_SIMULATION
#include <condition_variable>

#include "platform/virtual_line.hpp"
#endif

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
class BusEsp {
 public:
  using ReadListener = std::function<void(const uint8_t& byte)>;
  using WriteListener = std::function<void(const uint8_t& byte)>;
  using SynListener = std::function<void()>;

  explicit BusEsp(const BusConfig& config, const RuntimeConfig& runtime,
                  detail::Request* request, detail::BusMonitor* monitor);
  ~BusEsp();

  BusEsp(const BusEsp&) = delete;
  BusEsp& operator=(const BusEsp&) = delete;

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

  platform::ServiceThread::Status getThreadStatus() const;
  platform::ServiceThread::Status getSynThreadStatus() const;

  ebus::BusStatus getStatus() const;

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

  // owned queue
  std::unique_ptr<Queue<BusEvent>> byte_queue_;

  std::unique_ptr<ServiceThread> syn_worker_;

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

  uint8_t buffer_index_ = 0;  // Index for falling edge buffer
  int64_t micros_edge_buffer_[FALLING_EDGE_BUFFER_SIZE] = {0};
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

#if EBUS_SIMULATION
  // Simulation SYN generator state
  std::mutex syn_mutex_;
  std::condition_variable syn_cv_;
  Clock::time_point last_activity_time_;
  Clock::time_point next_syn_expiry_;
  Clock::time_point syn_intent_time_sim_;
  SemaphoreHandle_t sim_write_sem_ = nullptr;
  esp_timer_handle_t sim_write_timer_ = nullptr;
#endif

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

  // UART event task: reads UART events and pushes bytes to byteQueue
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  static void s_onSynPostpone(void* arg);

#if EBUS_SIMULATION
  // Simulation workers
  void simulationReaderLoop();
  void simulationSynLoop();
  void resetSynTimerSim(uint8_t byte);
#endif

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
