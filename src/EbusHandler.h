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

// Implementation of the sending routines for Master-Slave telegrams based on
// the ebus classes Telegram and Sequence. It also collects statistical data
// about the ebus system.

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Telegram.h"

namespace ebus {

struct Counters {
  // messages
  uint32_t messagesTotal = 0;

  uint32_t messagesPassiveMS = 0;
  uint32_t messagesPassiveMM = 0;

  uint32_t messagesReactiveMS = 0;
  uint32_t messagesReactiveMM = 0;
  uint32_t messagesReactiveBC = 0;

  uint32_t messagesActiveMS = 0;
  uint32_t messagesActiveMM = 0;
  uint32_t messagesActiveBC = 0;

  // errors
  uint32_t errorsTotal = 0;

  uint32_t errorsPassive = 0;
  uint32_t errorsPassiveMaster = 0;
  uint32_t errorsPassiveMasterACK = 0;
  uint32_t errorsPassiveSlave = 0;
  uint32_t errorsPassiveSlaveACK = 0;
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
  uint32_t resetsPassive = 0;
  uint32_t resetsActive = 0;

  // requests
  uint32_t requestsTotal = 0;
  uint32_t requestsWon = 0;
  uint32_t requestsLost = 0;
  uint32_t requestsRetry = 0;
  uint32_t requestsError = 0;
};

enum class State {
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

static const char *stateString(State state) {
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

class EbusHandler {
 public:
  EbusHandler() = default;
  EbusHandler(const uint8_t source, std::function<bool()> busReadyFunction,
              std::function<void(const uint8_t byte)> busWriteFunction,
              std::function<void(const std::vector<uint8_t> master,
                                 const std::vector<uint8_t> slave)>
                  activeFunction,
              std::function<void(const std::vector<uint8_t> master,
                                 const std::vector<uint8_t> slave)>
                  passiveFunction,
              std::function<void(const std::vector<uint8_t> master,
                                 std::vector<uint8_t> *const slave)>
                  reactiveFunction);

  void setErrorCallback(
      std::function<void(const std::string str)> errorFunction);

  void setAddress(const uint8_t source);
  uint8_t getAddress() const;
  uint8_t getSlaveAddress() const;

  void setMaxLockCounter(const uint8_t counter);

  void setExternalBusRequest(const bool external);

  State getState() const;
  bool isActive() const;

  void reset();
  bool enque(const std::vector<uint8_t> &message);

  void wonExternalBusRequest(const bool won);

  void run(const uint8_t &byte);

  void resetCounters();
  const Counters &getCounters();

 private:
  uint8_t address = 0;
  uint8_t slaveAddress = 0;

  std::function<bool()> busReadyCallback = nullptr;
  std::function<void(const uint8_t byte)> busWriteCallback = nullptr;
  std::function<void(const std::vector<uint8_t> master,
                     const std::vector<uint8_t> slave)>
      activeCallback = nullptr;
  std::function<void(const std::vector<uint8_t> master,
                     const std::vector<uint8_t> slave)>
      passiveCallback = nullptr;
  std::function<void(const std::vector<uint8_t> master,
                     std::vector<uint8_t> *const slave)>
      reactiveCallback = nullptr;

  std::function<void(const std::string str)> errorCallback = nullptr;

  bool external = false;

  Counters counters;

  // control
  State state = State::passiveReceiveMaster;
  bool write = false;
  uint8_t maxLockCounter = 3;
  uint8_t lockCoutner = 0;

  // passive
  Telegram passiveTelegram;

  Sequence passiveMaster;
  size_t passiveMasterDBx = 0;
  bool passiveMasterRepeated = false;

  Sequence passiveSlave;
  size_t passiveSlaveDBx = 0;
  size_t passiveSlaveSendIndex = 0;
  size_t passiveSlaveReceiveIndex = 0;
  bool passiveSlaveRepeated = false;

  // active
  bool active = false;
  Telegram activeTelegram;

  Sequence activeMaster;
  size_t activeMasterSendIndex = 0;
  size_t activeMasterReceiveIndex = 0;
  bool activeMasterRepeated = false;

  Sequence activeSlave;
  size_t activeSlaveDBx = 0;
  bool activeSlaveRepeated = false;

  void receive(const uint8_t &byte);
  void send();

  void resetPassive();
  void resetActive();
};

}  // namespace ebus
