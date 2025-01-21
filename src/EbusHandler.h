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
  uint32_t total = 0;

  uint32_t passive = 0;
  float passivePercent = 0;

  uint32_t passiveMS = 0;
  uint32_t passiveMM = 0;
  uint32_t passiveBC = 0;

  uint32_t passiveMSAtMe = 0;
  uint32_t passiveMMAtMe = 0;

  uint32_t active = 0;
  float activePercent = 0;

  uint32_t activeMS = 0;
  uint32_t activeMM = 0;
  uint32_t activeBC = 0;

  uint32_t failure = 0;
  float failurePercent = 0;

  uint32_t requestTotal = 0;

  uint32_t requestWon = 0;
  float requestWonPercent = 0;
  uint32_t requestWon1 = 0;
  uint32_t requestWon2 = 0;
  uint32_t requestRetry = 0;

  uint32_t requestLost = 0;
  float requestLostPercent = 0;
  uint32_t requestLost1 = 0;
  uint32_t requestLost2 = 0;

  uint32_t requestError = 0;
  float requestErrorPercent = 0;
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

  void setTraceCallback(std::function<void(const char *)> traceFunction);

  void setAddress(const uint8_t source);
  uint8_t getAddress() const;
  uint8_t getSlaveAddress() const;

  void setMaxLockCounter(const uint8_t counter);

  State getState() const;
  bool isActive() const;

  void reset();
  bool enque(const std::vector<uint8_t> &message);

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

  std::function<void(const char *)> traceCallback = nullptr;

  State state = State::passiveReceiveMaster;

  Counters counters;

  // control
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
