/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <ebus/Definitions.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Core/Request.hpp"
#include "Core/Telegram.hpp"
#include "Platform/Bus.hpp"
#include "Platform/Queue.hpp"
#include "Utils/TimingStats.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_ADDRESS = 0xff;

constexpr size_t NUM_HANDLER_STATES = 15;

enum class HandlerState {
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

static const char* getHandlerStateText(HandlerState state) {
  const char* values[] = {"passiveReceiveMaster",
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

using BusRequestWonCallback = std::function<void()>;
using BusRequestLostCallback = std::function<void()>;

using ReactiveMasterSlaveCallback = std::function<void(
    const std::vector<uint8_t>& master, std::vector<uint8_t>* const slave)>;

#define EBUS_HANDLER_COUNTER_LIST \
  X(messagesPassiveMasterSlave)   \
  X(messagesPassiveMasterMaster)  \
  X(messagesPassiveBroadcast)     \
  X(messagesActiveMasterSlave)    \
  X(messagesActiveMasterMaster)   \
  X(messagesActiveBroadcast)      \
  X(messagesReactiveMasterSlave)  \
  X(messagesReactiveMasterMaster) \
  X(resetPassive00)               \
  X(resetPassive0704)             \
  X(resetPassive)                 \
  X(resetActive00)                \
  X(resetActive0704)              \
  X(resetActive)                  \
  X(errorPassiveMaster)           \
  X(errorPassiveMasterACK)        \
  X(errorPassiveSlave)            \
  X(errorPassiveSlaveACK)         \
  X(errorReactiveMaster)          \
  X(errorReactiveMasterACK)       \
  X(errorReactiveSlave)           \
  X(errorReactiveSlaveACK)        \
  X(errorActiveMaster)            \
  X(errorActiveMasterACK)         \
  X(errorActiveSlave)             \
  X(errorActiveSlaveACK)

#define EBUS_HANDLER_TIMING_LIST \
  X(sync)                        \
  X(write)                       \
  X(passiveFirst)                \
  X(passiveData)                 \
  X(activeFirst)                 \
  X(activeData)                  \
  X(callbackWon)                 \
  X(callbackLost)                \
  X(callbackReactive)            \
  X(callbackTelegram)            \
  X(callbackError)

/**
 * Handler class that implements the eBUS protocol logic as a finite state
 * machine. It processes incoming bytes from the bus, manages state transitions,
 * and invokes callbacks for telegrams and errors. It also collects various
 * metrics about the bus activity. The handler maintains separate buffers and
 * state for passive (observing) and active (transmitting) telegrams, and can
 * react to bus events by sending messages or requesting the bus.
 */
class Handler {
 public:
  Handler(const uint8_t& address, Bus* bus, Request* request);

  void setSourceAddress(const uint8_t& address);
  uint8_t getSourceAddress() const;
  uint8_t getTargetAddress() const;

  void setBusRequestWonCallback(BusRequestWonCallback callback);
  void setBusRequestLostCallback(BusRequestLostCallback callback);
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  HandlerState getState() const;

  bool sendActiveMessage(const std::vector<uint8_t>& message);
  bool isActiveMessagePending() const;

  void reset();

  void run(const BusEventContext& ctx);

  void resetMetrics();
  std::map<std::string, MetricValues> getMetrics() const;

 private:
  Bus* bus_ = nullptr;
  Request* request_ = nullptr;
  RequestResult lastResult_ = RequestResult::observeSyn;

  uint8_t sourceAddress_ = 0;
  uint8_t targetAddress_ = 0;

  BusRequestWonCallback busRequestWonCallback_ = nullptr;
  BusRequestLostCallback busRequestLostCallback_ = nullptr;
  ReactiveMasterSlaveCallback reactiveMasterSlaveCallback_ = nullptr;
  TelegramCallback telegramCallback_ = nullptr;
  ErrorCallback errorCallback_ = nullptr;

  std::array<void (Handler::*)(const uint8_t&), NUM_HANDLER_STATES>
      stateHandlers_ = {};

  HandlerState state_ = HandlerState::passiveReceiveMaster;
  HandlerState lastState_ = HandlerState::passiveReceiveMaster;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_HANDLER_COUNTER_LIST
#undef X
  };

  Counter counter_;

  TimingStats sync_;
  TimingStats write_;
  TimingStats passiveFirst_;
  TimingStats passiveData_;
  TimingStats activeFirst_;
  TimingStats activeData_;
  TimingStats callbackWon_;
  TimingStats callbackLost_;
  TimingStats callbackReactive_;
  TimingStats callbackTelegram_;
  TimingStats callbackError_;

  std::array<TimingStats, NUM_HANDLER_STATES> handlerTiming_ = {};

  std::chrono::steady_clock::time_point lastPoint_;
  bool measureSync_ = false;

  // passive
  Telegram passiveTelegram_;

  Sequence passiveMaster_;
  size_t passiveMasterDBx_ = 0;
  bool passiveMasterRepeated_ = false;

  Sequence passiveSlave_;
  size_t passiveSlaveDBx_ = 0;
  size_t passiveSlaveIndex_ = 0;
  bool passiveSlaveRepeated_ = false;

  // active
  bool activeMessage_ = false;
  Telegram activeTelegram_;

  Sequence activeMaster_;
  size_t activeMasterIndex_ = 0;
  bool activeMasterRepeated_ = false;

  Sequence activeSlave_;
  size_t activeSlaveDBx_ = 0;
  bool activeSlaveRepeated_ = false;

  void passiveReceiveMaster(const uint8_t& byte);
  void passiveReceiveMasterAcknowledge(const uint8_t& byte);
  void passiveReceiveSlave(const uint8_t& byte);
  void passiveReceiveSlaveAcknowledge(const uint8_t& byte);
  void reactiveSendMasterPositiveAcknowledge(const uint8_t& byte);
  void reactiveSendMasterNegativeAcknowledge(const uint8_t& byte);
  void reactiveSendSlave(const uint8_t& byte);
  void reactiveReceiveSlaveAcknowledge(const uint8_t& byte);
  void requestBus(const uint8_t& byte);
  void activeSendMaster(const uint8_t& byte);
  void activeReceiveMasterAcknowledge(const uint8_t& byte);
  void activeReceiveSlave(const uint8_t& byte);
  void activeSendSlavePositiveAcknowledge(const uint8_t& byte);
  void activeSendSlaveNegativeAcknowledge(const uint8_t& byte);
  void releaseBus(const uint8_t& byte);

  void checkPassiveBuffers();
  void checkActiveBuffers();

  void callPassiveReset();
  void callActiveReset();

  void callWrite(const uint8_t& byte);

  void callOnBusRequestWon();
  void callOnBusRequestLost();

  void callOnReactiveMasterSlave(const std::vector<uint8_t>& master,
                                 std::vector<uint8_t>* const slave);

  void callOnTelegram(const MessageType& messageType,
                      const TelegramType& telegramType,
                      const std::vector<uint8_t>& master,
                      const std::vector<uint8_t>& slave);

  void callOnError(const std::string& error, const std::vector<uint8_t>& master,
                   const std::vector<uint8_t>& slave);
};

}  // namespace ebus
