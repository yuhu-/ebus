/*
 * Copyright (C) 2023-2025 Roland Jax
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

// Implementation of the send and receive routines for all types of telegrams on
// the basis of a finite state machine. A large number of counters and
// statistical data about the eBUS system are collected.

#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Bus.hpp"
#include "Queue.hpp"
#include "Request.hpp"
#include "Telegram.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_ADDRESS = 0xff;

constexpr uint8_t DEFAULT_LOCK_COUNTER = 3;
constexpr uint8_t MAX_LOCK_COUNTER = 25;

constexpr size_t NUM_FSM_STATES = 15;

enum class FsmState {
  passiveReceiveMaster,
  passiveReceiveMasterAcknowledge,
  passiveReceiveSlave,
  passiveReceiveSlaveAcknowledge,
  reactiveSendMasterPositiveAcknowledge,
  reactiveSendMasterNegativeAcknowledge,
  reactiveSendSlave,
  reactiveReceiveSlaveAcknowledge,
  requestBus,
  activeSendMaster,
  activeReceiveMasterAcknowledge,
  activeReceiveSlave,
  activeSendSlavePositiveAcknowledge,
  activeSendSlaveNegativeAcknowledge,
  releaseBus
};

static const char *getFsmStateText(FsmState state) {
  const char *values[] = {"passiveReceiveMaster",
                          "passiveReceiveMasterAcknowledge",
                          "passiveReceiveSlave",
                          "passiveReceiveSlaveAcknowledge",
                          "reactiveSendMasterPositiveAcknowledge",
                          "reactiveSendMasterNegativeAcknowledge",
                          "reactiveSendSlave",
                          "reactiveReceiveSlaveAcknowledge",
                          "requestBus",
                          "activeSendMaster",
                          "activeReceiveMasterAcknowledge",
                          "activeReceiveSlave",
                          "activeSendSlavePositiveAcknowledge",
                          "activeSendSlaveNegativeAcknowledge",
                          "releaseBus"};
  return values[static_cast<int>(state)];
}

enum class MessageType { undefined, active, passive, reactive };

using ReactiveMasterSlaveCallback = std::function<void(
    const std::vector<uint8_t> &master, std::vector<uint8_t> *const slave)>;

using TelegramCallback = std::function<void(
    const MessageType &messageType, const TelegramType &telegramType,
    const std::vector<uint8_t> &master, const std::vector<uint8_t> &slave)>;

using ErrorCallback = std::function<void(const std::string &errorMessage,
                                         const std::vector<uint8_t> &master,
                                         const std::vector<uint8_t> &slave)>;

#define EBUS_COUNTERS_LIST        \
  X(messagesTotal)                \
  X(messagesPassiveMasterSlave)   \
  X(messagesPassiveMasterMaster)  \
  X(messagesPassiveBroadcast)     \
  X(messagesActiveMasterSlave)    \
  X(messagesActiveMasterMaster)   \
  X(messagesActiveBroadcast)      \
  X(messagesReactiveMasterSlave)  \
  X(messagesReactiveMasterMaster) \
  X(requestsTotal)                \
  X(requestsWon1)                 \
  X(requestsWon2)                 \
  X(requestsLost1)                \
  X(requestsLost2)                \
  X(requestsError1)               \
  X(requestsError2)               \
  X(requestsErrorRetry)           \
  X(requestsStartBit)             \
  X(resetsTotal)                  \
  X(resetsPassive00)              \
  X(resetsPassive0704)            \
  X(resetsPassive)                \
  X(resetsActive)                 \
  X(errorsTotal)                  \
  X(errorsPassive)                \
  X(errorsPassiveMaster)          \
  X(errorsPassiveMasterACK)       \
  X(errorsPassiveSlave)           \
  X(errorsPassiveSlaveACK)        \
  X(errorsReactive)               \
  X(errorsReactiveMaster)         \
  X(errorsReactiveMasterACK)      \
  X(errorsReactiveSlave)          \
  X(errorsReactiveSlaveACK)       \
  X(errorsActive)                 \
  X(errorsActiveMaster)           \
  X(errorsActiveMasterACK)        \
  X(errorsActiveSlave)            \
  X(errorsActiveSlaveACK)

struct Counters {
#define X(name) uint32_t name = 0;
  EBUS_COUNTERS_LIST
#undef X
};

#define EBUS_TIMINGS_LIST \
  X(sync)                 \
  X(write)                \
  X(busIsrDelay)          \
  X(busIsrWindow)         \
  X(passiveFirst)         \
  X(passiveData)          \
  X(activeFirst)          \
  X(activeData)           \
  X(callbackReactive)     \
  X(callbackTelegram)     \
  X(callbackError)

struct Timings {
#define X(name)             \
  double name##Last = 0;    \
  uint64_t name##Count = 0; \
  double name##Mean = 0;    \
  double name##StdDev = 0;
  EBUS_TIMINGS_LIST
#undef X
};

struct TimingStats {
  double last = 0;  // holds the last value added
  uint64_t count = 0;
  double mean = 0;
  double m2 = 0;  // for variance

  void add(double x) {
    last = x;
    ++count;
    double delta = x - mean;
    mean += delta / count;
    double delta2 = x - mean;
    m2 += delta * delta2;
  }
  double variance() const { return count > 1 ? m2 / (count - 1) : 0; }
  double stddev() const { return sqrt(variance()); }
  void clear() {
    last = 0;
    count = 0;
    mean = 0;
    m2 = 0;
  }
};

struct StateTimingStatsResults {
  struct StateStats {
    std::string name;
    double last;
    double mean;
    double stddev;
    uint64_t count;
  };
  std::map<FsmState, StateStats> states;
};

class Handler {
 public:
  explicit Handler(Bus *device, const uint8_t source);

  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  void setAddress(const uint8_t source);
  uint8_t getAddress() const;
  uint8_t getSlaveAddress() const;

  void setMaxLockCounter(const uint8_t counter);

  FsmState getState() const;
  bool isActive() const;

  void microsBusIsrDelay(const int64_t &delay);
  void microsBusIsrWindow(const int64_t &window);

  bool busRequest() const;
  void busRequested();
  void busIsrStartBit();

  void reset();
  bool enque(const std::vector<uint8_t> &message);

  void run(const uint8_t &byte);

  void resetCounters();
  const Counters &getCounters();

  void resetTimings();
  const Timings &getTimings();

  void resetStateTimingStats();
  const StateTimingStatsResults getStateTimingStatsResults() const;

 private:
  Bus *bus = nullptr;

  uint8_t address = 0;
  uint8_t slaveAddress = 0;

  std::array<void (Handler::*)(const uint8_t &), NUM_FSM_STATES> stateHandlers;

  ReactiveMasterSlaveCallback reactiveMasterSlaveCallback = nullptr;
  TelegramCallback telegramCallback = nullptr;
  ErrorCallback errorCallback = nullptr;

  // request
  bool request = false;
  Request requestHandler;

  FsmState state = FsmState::passiveReceiveMaster;
  FsmState lastState = FsmState::passiveReceiveMaster;

  uint8_t maxLockCounter = DEFAULT_LOCK_COUNTER;
  uint8_t lockCounter = DEFAULT_LOCK_COUNTER;

  // measurement
  Counters counters;
  Timings timings;
  std::array<TimingStats, NUM_FSM_STATES> fsmTimingStats = {};

  std::chrono::steady_clock::time_point lastPoint;
  bool measureSync = false;

  TimingStats sync;
  TimingStats write;
  TimingStats busIsrDelay;
  TimingStats busIsrWindow;
  TimingStats passiveFirst;
  TimingStats passiveData;
  TimingStats activeFirst;
  TimingStats activeData;
  TimingStats callbackReactive;
  TimingStats callbackTelegram;
  TimingStats callbackError;

  // passive
  Telegram passiveTelegram;

  Sequence passiveMaster;
  size_t passiveMasterDBx = 0;
  bool passiveMasterRepeated = false;

  Sequence passiveSlave;
  size_t passiveSlaveDBx = 0;
  size_t passiveSlaveIndex = 0;
  bool passiveSlaveRepeated = false;

  // active
  bool active = false;
  Telegram activeTelegram;

  Sequence activeMaster;
  size_t activeMasterIndex = 0;
  bool activeMasterRepeated = false;

  Sequence activeSlave;
  size_t activeSlaveDBx = 0;
  bool activeSlaveRepeated = false;

  void passiveReceiveMaster(const uint8_t &byte);
  void passiveReceiveMasterAcknowledge(const uint8_t &byte);
  void passiveReceiveSlave(const uint8_t &byte);
  void passiveReceiveSlaveAcknowledge(const uint8_t &byte);
  void reactiveSendMasterPositiveAcknowledge(const uint8_t &byte);
  void reactiveSendMasterNegativeAcknowledge(const uint8_t &byte);
  void reactiveSendSlave(const uint8_t &byte);
  void reactiveReceiveSlaveAcknowledge(const uint8_t &byte);
  void requestBus(const uint8_t &byte);
  void activeSendMaster(const uint8_t &byte);
  void activeReceiveMasterAcknowledge(const uint8_t &byte);
  void activeReceiveSlave(const uint8_t &byte);
  void activeSendSlavePositiveAcknowledge(const uint8_t &byte);
  void activeSendSlaveNegativeAcknowledge(const uint8_t &byte);
  void releaseBus(const uint8_t &byte);

  void checkPassiveBuffers();
  void checkActiveBuffers();

  void callPassiveReset();
  void callActiveReset();

  void calculateDuration(const uint8_t &byte);
  void calculateDurationFsmState(const uint8_t &byte);

  void callWrite(const uint8_t &byte);

  void callReactiveMasterSlave(const std::vector<uint8_t> &master,
                               std::vector<uint8_t> *const slave);

  void callOnTelegram(const MessageType &messageType,
                      const TelegramType &telegramType,
                      const std::vector<uint8_t> &master,
                      const std::vector<uint8_t> &slave);

  void callOnError(const std::string &error, const std::vector<uint8_t> &master,
                   const std::vector<uint8_t> &slave);
};

}  // namespace ebus
