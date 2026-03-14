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

#pragma once

#if defined(ESP32)
#include <cstdint>
#include <functional>

#include "../Queue.hpp"
#include "../Request.hpp"
#include "../TimingStats.hpp"
#include "driver/uart.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/gptimer.h"
#else
#include "driver/timer.h"
#endif

namespace ebus {

struct busConfig {
  uint8_t uart_port;
  uint8_t rx_pin;
  uint8_t tx_pin;
  uint8_t timer_group;
  uint8_t timer_idx;
};

struct BusEvent {
  uint8_t byte;
  bool busRequest{false};
  bool startBit{false};
};

#define EBUS_BUS_COUNTER_LIST X(busStartBit)

#define EBUS_BUS_TIMING_LIST \
  X(busDelay_)               \
  X(busWindow_)

class BusFreeRtos {
 public:
  // measurement
  struct Counter {
#define X(name) uint32_t name = 0;
    EBUS_BUS_COUNTER_LIST
#undef X
  };

  struct Timing {
#define X(name)             \
  double name##Last = 0;    \
  uint64_t name##Count = 0; \
  double name##Mean = 0;    \
  double name##StdDev = 0;
    EBUS_BUS_TIMING_LIST
#undef X
  };

  explicit BusFreeRtos(const busConfig& config, Request* request);
  ~BusFreeRtos();

  BusFreeRtos(const BusFreeRtos&) = delete;
  BusFreeRtos& operator=(const BusFreeRtos&) = delete;

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);

  // FreeRtos specific
  void resetCounter();
  const Counter& getCounter() const;

  void resetTiming();
  const Timing& getTiming();

 private:
  uart_port_t uartPortNum_;
  uint8_t rxPin_;
  uint8_t txPin_;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  gptimer_handle_t gptimer_ = nullptr;
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

  volatile int64_t microsStartBit_ = 0;  // estimated start bit time

  volatile bool busRequestFlag_ = false;
  volatile bool startBitFlag_ = false;

  volatile bool microsDelayFlag_ = false;
  volatile bool microsWindowFlag_ = false;

  volatile int64_t microsLastDelay_ = 0;
  volatile int64_t microsLastWindow_ = 0;

  // platform handles
  QueueHandle_t uartEventQueue_ = nullptr;
  portMUX_TYPE timerMux_ = portMUX_INITIALIZER_UNLOCKED;

  // measurement
  Counter counter_;
  Timing timing_;

  TimingStats busDelay_;
  TimingStats busWindow_;

  // setup helpers
  void configureUart();
  void configureGpio();
  void configureTimer();

  // UART event task: reads UART events and pushes bytes to byteQueue
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  // ISR: Save the falling edges in order to estimate the sync byte
  static void IRAM_ATTR s_onFallingEdge(void* arg);
  void onFallingEdge();

  // ISR: Write request byte at the exact time
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static bool IRAM_ATTR s_onBusIsrTimer(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t* edata,
                                        void* user_ctx);
#else
  static bool IRAM_ATTR s_onBusIsrTimer(void* arg);
#endif
  bool onBusIsrTimer();
};

}  // namespace ebus

#endif
