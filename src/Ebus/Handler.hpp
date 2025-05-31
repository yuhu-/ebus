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

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Telegram.hpp"

namespace ebus {

struct Counters {
  // messages
  uint32_t messagesTotal = 0;

  uint32_t messagesPassiveMasterSlave = 0;
  uint32_t messagesPassiveMasterMaster = 0;

  uint32_t messagesReactiveMasterSlave = 0;
  uint32_t messagesReactiveMasterMaster = 0;
  uint32_t messagesReactiveBroadcast = 0;

  uint32_t messagesActiveMasterSlave = 0;
  uint32_t messagesActiveMasterMaster = 0;
  uint32_t messagesActiveBroadcast = 0;

  // errors
  uint32_t errorsTotal = 0;

  uint32_t errorsPassive = 0;
  uint32_t errorsPassiveMaster = 0;
  uint32_t errorsPassiveMasterACK = 0;
  uint32_t errorsPassiveSlave = 0;
  uint32_t errorsPassiveSlaveACK = 0;

  uint32_t errorsReactive = 0;
  uint32_t errorsReactiveMaster = 0;
  uint32_t errorsReactiveMasterACK = 0;
  uint32_t errorsReactiveSlave = 0;
  uint32_t errorsReactiveSlaveACK = 0;

  uint32_t errorsActive = 0;
  uint32_t errorsActiveMaster = 0;
  uint32_t errorsActiveMasterACK = 0;
  uint32_t errorsActiveSlave = 0;
  uint32_t errorsActiveSlaveACK = 0;

  // resets
  uint32_t resetsTotal = 0;
  uint32_t resetsPassive00 = 0;
  uint32_t resetsPassive0704 = 0;
  uint32_t resetsPassive = 0;
  uint32_t resetsActive = 0;

  // requests
  uint32_t requestsTotal = 0;
  uint32_t requestsWon = 0;
  uint32_t requestsLost = 0;
  uint32_t requestsRetry = 0;
  uint32_t requestsError = 0;
};

constexpr uint8_t DEFAULT_ADDRESS = 0xff;

constexpr uint8_t DEFAULT_LOCK_COUNTER = 3;
constexpr uint8_t MAX_LOCK_COUNTER = 25;

enum class FsmState {
  passiveReceiveMaster,
  passiveReceiveMasterAcknowledge,
  passiveReceiveSlave,
  passiveReceiveSlaveAcknowledge,
  reactiveSendMasterPositiveAcknowledge,
  reactiveSendMasterNegativeAcknowledge,
  reactiveSendSlave,
  reactiveReceiveSlaveAcknowledge,
  requestBusFirstTry,
  requestBusPriorityRetry,
  requestBusSecondTry,
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
                          "requestBusFirstTry",
                          "requestBusPriorityRetry",
                          "requestBusSecondTry",
                          "activeSendMaster",
                          "activeReceiveMasterAcknowledge",
                          "activeReceiveSlave",
                          "activeSendSlavePositiveAcknowledge",
                          "activeSendSlaveNegativeAcknowledge",
                          "releaseBus"};
  return values[static_cast<int>(state)];
}

enum class MessageType { undefined, active, passive, reactive };

typedef std::function<void(const uint8_t byte)> OnWriteCallback;
typedef std::function<int()> IsDataAvailableCallback;

typedef std::function<void(const MessageType &message, const TelegramType &type,
                           const std::vector<uint8_t> &master,
                           std::vector<uint8_t> *const slave)>
    OnTelegramCallback;

typedef std::function<void(const std::string str)> OnErrorCallback;

class Handler {
 public:
  Handler() = default;
  explicit Handler(const uint8_t source);

  void onWrite(ebus::OnWriteCallback callback);
  void isDataAvailable(ebus::IsDataAvailableCallback callback);
  void onTelegram(ebus::OnTelegramCallback callback);
  void onError(ebus::OnErrorCallback callback);

  void setAddress(const uint8_t source);
  uint8_t getAddress() const;
  uint8_t getSlaveAddress() const;

  void setMaxLockCounter(const uint8_t counter);

  FsmState getState() const;
  bool isActive() const;

  void reset();
  bool enque(const std::vector<uint8_t> &message);

  void run(const uint8_t &byte);

  void resetCounters();
  const Counters &getCounters();

 private:
  uint8_t address = 0;
  uint8_t slaveAddress = 0;

  ebus::OnWriteCallback onWriteCallback = nullptr;
  ebus::IsDataAvailableCallback isDataAvailableCallback = nullptr;
  ebus::OnTelegramCallback onTelegramCallback = nullptr;
  ebus::OnErrorCallback onErrorCallback = nullptr;

  Counters counters;

  // control
  FsmState state = FsmState::passiveReceiveMaster;
  uint8_t maxLockCounter = DEFAULT_LOCK_COUNTER;
  uint8_t lockCounter = DEFAULT_LOCK_COUNTER;

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
  void requestBusFirstTry(const uint8_t &byte);
  void requestBusPriorityRetry(const uint8_t &byte);
  void requestBusSecondTry(const uint8_t &byte);
  void activeSendMaster(const uint8_t &byte);
  void activeReceiveMasterAcknowledge(const uint8_t &byte);
  void activeReceiveSlave(const uint8_t &byte);
  void activeSendSlavePositiveAcknowledge(const uint8_t &byte);
  void activeSendSlaveNegativeAcknowledge(const uint8_t &byte);
  void releaseBus(const uint8_t &byte);

  void handlePassiveErrors(const FsmState &lastState);
  void handleActiveErrors(const FsmState &lastState);

  void resetPassive();
  void resetActive();
};

}  // namespace ebus
