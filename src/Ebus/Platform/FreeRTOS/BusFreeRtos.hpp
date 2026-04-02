/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP32)
#include <cstdint>
#include <ebus/Config.hpp>
#include <functional>
#include <map>

#include "Core/Request.hpp"
#include "Platform/Queue.hpp"
#include "Utils/TimingStats.hpp"
#include "driver/uart.h"
#include "esp_idf_version.h"

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
  X(statsDelay)              \
  X(statsWindow)             \
  X(statsTransmit)           \
  X(statsUptime)

class BusFreeRtos {
 public:
  explicit BusFreeRtos(const busConfig& config, const RuntimeConfig& runtime,
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
  uart_port_t uartPortNum_;
  uint8_t rxPin_;
  uint8_t txPin_;

  busConfig config_;
  RuntimeConfig runtime_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  gptimer_handle_t gptimer_ = nullptr;
  gptimer_handle_t synGptimer_ = nullptr;
#else
  timer_group_t timerGroupNum_ = TIMER_GROUP_1;
  timer_idx_t timerIdxNum_ = TIMER_0;
#endif

  Request* request_ = nullptr;

  // owned queue
  Queue<BusEvent>* byteQueue_ = nullptr;

  // ISR/state
  static constexpr uint8_t FALLING_EDGE_BUFFER_SIZE = 5;

  // The bit time at 2400 baud is approximately 416.67 us
  static constexpr float bitTime_ = 1000000.0 / 2400.0;  // ~416.67 us

  // The byte time at 2400 baud for 10 bits with a 0.5-bit offset is
  // approximately 9.5 * bitTime_ = 9.5 * 416.67 us = 3958.33 us
  static constexpr int64_t byteTime_ = 9.5 * bitTime_;  // ~3958.33 us

  // This value can be adjusted if the bus ISR is not working as expected.
  volatile uint16_t window_ = 4300;  // usually between 4300-4456 us
  volatile uint16_t offset_ = 80;    // mainly for context switch and write

  volatile uint8_t bufferIndex_ = 0;  // index for falling edge buffer
  volatile int64_t microsEdgeBuffer_[FALLING_EDGE_BUFFER_SIZE] = {0};

  // volatile int64_t lastActivityMicros_ = 0;
  volatile int64_t microsStartBit_ = 0;  // estimated start bit time

  volatile bool busRequestFlag_ = false;
  volatile bool startBitFlag_ = false;

  volatile bool microsDelayFlag_ = false;
  volatile bool microsWindowFlag_ = false;

  volatile int64_t microsLastDelay_ = 0;
  volatile int64_t microsLastWindow_ = 0;

  std::atomic<bool> synRunning_{false};
  bool synActive_{false};
  uint64_t synBaseUs_ = 0;
  uint64_t synUniqueUs_ = 0;

  // platform handles
  QueueHandle_t uartEventQueue_ = nullptr;
  portMUX_TYPE timerMux_ = portMUX_INITIALIZER_UNLOCKED;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_BUS_COUNTER_LIST
#undef X
  };

  Counter counter_;

  TimingStats statsDelay_;
  TimingStats statsWindow_;
  TimingStats statsTransmit_;
  RollingStats statsUtilization_;
  TimingStats statsUptime_;

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
