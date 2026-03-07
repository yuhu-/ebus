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
#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/uart.h"

namespace ebus {

typedef struct {
  uint8_t uart_port;
  uint8_t rx_pin;
  uint8_t tx_pin;
  uint8_t timer_group;
  uint8_t timer_idx;
} bus_config_t;

struct BusEvent {
  uint8_t byte;
  bool busRequest{false};
  bool startBit{false};
  // bool microsDelay{false};
  // int64_t microsLastDelay{0};
  // bool microsWindow{false};
  // int64_t microsLastWindow{0};
};

using BusRequestPendingCallback = std::function<bool()>;
using RequestAddressCallback = std::function<const uint8_t&()>;

#define EBUS_BUS_COUNTER_LIST X(busStartBit)

#define EBUS_BUS_TIMING_LIST \
  X(busIsrDelay)                \
  X(busIsrWindow)

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

  explicit BusFreeRtos(bus_config_t& config, Request& request);
  ~BusFreeRtos();

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  // FreeRtos specific
  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);

  void resetCounter();
  const Counter& getCounter() const;

  void resetTiming();
  const Timing& getTiming();

 private:
  BusFreeRtos(const BusFreeRtos&) = delete;
  BusFreeRtos& operator=(const BusFreeRtos&) = delete;

  // configuration
  uart_port_t m_uartPortNum;
  uint8_t m_rxPin;
  uint8_t m_txPin;
  timer_group_t m_timerGroupNum = TIMER_GROUP_1;
  timer_idx_t m_timerIdxNum = TIMER_0;

  Request& m_request;

  // owned queue
  Queue<BusEvent>* m_byteQueue = nullptr;

  // ISR/state
  static constexpr uint8_t FALLING_EDGE_BUFFER_SIZE = 5;

  // The bit time at 2400 baud is approximately 416.67 us
  static constexpr float bit_time = 1000000.0 / 2400.0;  // ~416.67 us

  // The byte time at 2400 baud for 10 bits with a 0.5-bit offset is
  // approximately 9.5 * bit_time = 9.5 * 416.67 us = 3958.33 us
  static constexpr int64_t byte_time = 9.5 * bit_time;  // ~3958.33 us

  // This value can be adjusted if the bus ISR is not working as expected.
  volatile uint16_t m_busIsrWindow = 4300;  // usually between 4300-4456 us
  volatile uint16_t m_busIsrOffset = 80;  // mainly for context switch and write

  volatile uint8_t m_bufferIndex = 0;  // index for falling edge buffer
  volatile int64_t m_microsEdgeBuffer[FALLING_EDGE_BUFFER_SIZE] = {0};

  volatile int64_t m_microsStartBit = 0;  // estimated start bit time

  volatile bool m_busRequestFlag = false;
  volatile bool m_startBitFlag = false;

  volatile bool m_microsDelayFlag = false;
  volatile bool m_microsWindowFlag = false;

  volatile int64_t m_microsLastDelay = 0;
  volatile int64_t m_microsLastWindow = 0;

  // platform handles
  QueueHandle_t m_uartEventQueue = nullptr;
  portMUX_TYPE m_timerMux = portMUX_INITIALIZER_UNLOCKED;

  // measurement
  Counter counter;
  Timing timing;

  TimingStats busIsrDelay;
  TimingStats busIsrWindow;

  // setup helpers
  void configureUart();
  void configureGpio();
  void configureTimer();

  // UART event task: reads UART events and pushes bytes to byteQueue
  static void s_ebusUartEventRunner(void* arg);
  void ebusUartEventRunner();  // instance worker used by static trampoline

  // ISR: Save the falling edges in order to estimate the sync byte
  static void IRAM_ATTR s_onFallingEdge(void* arg);
  void IRAM_ATTR onFallingEdge();

  // ISR: Write request byte at the exact time
  static bool IRAM_ATTR s_onBusIsrTimer(void* arg);
  bool IRAM_ATTR onBusIsrTimer();
};

}  // namespace ebus

#endif
