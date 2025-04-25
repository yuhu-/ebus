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

#include "EbusHandler.h"

#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include "Telegram.h"

ebus::EbusHandler::EbusHandler(
    const uint8_t source, std::function<void(const uint8_t byte)> writeFunction,
    std::function<int()> readBufferFunction,
    std::function<void(const Message &message, const Type &type,
                       const std::vector<uint8_t> &master,
                       std::vector<uint8_t> *const slave)>
        publishFunction) {
  setAddress(source);
  writeCallback = writeFunction;
  readBufferCallback = readBufferFunction;
  publishCallback = publishFunction;
}

void ebus::EbusHandler::setErrorCallback(
    std::function<void(const std::string)> errorFunction) {
  errorCallback = errorFunction;
}

void ebus::EbusHandler::setAddress(const uint8_t source) {
  if (ebus::Telegram::isMaster(source))
    address = source;
  else
    address = 0xff;

  slaveAddress = Telegram::slaveAddress(address);
}

uint8_t ebus::EbusHandler::getAddress() const { return address; }

uint8_t ebus::EbusHandler::getSlaveAddress() const { return slaveAddress; }

void ebus::EbusHandler::setMaxLockCounter(const uint8_t counter) {
  if (counter > 25)
    maxLockCounter = 3;
  else
    maxLockCounter = counter;

  lockCoutner = maxLockCounter;
}

ebus::State ebus::EbusHandler::getState() const { return state; }

bool ebus::EbusHandler::isActive() const { return active; }

void ebus::EbusHandler::reset() {
  state = State::passiveReceiveMaster;
  resetActive();
  resetPassive();
}

bool ebus::EbusHandler::enque(const std::vector<uint8_t> &message) {
  active = false;
  activeTelegram.createMaster(address, message);
  if (activeTelegram.getMasterState() == SEQ_OK)
    active = true;
  else
    counters.errorsActiveMaster++;

  return active;
}

void ebus::EbusHandler::run(const uint8_t &byte) {
  receive(byte);
  send();
}

void ebus::EbusHandler::resetCounters() {
  // messages
  counters.messagesTotal = 0;

  counters.messagesPassiveMasterSlave = 0;
  counters.messagesPassiveMasterMaster = 0;

  counters.messagesReactiveMasterSlave = 0;
  counters.messagesReactiveMasterMaster = 0;
  counters.messagesReactiveBroadcast = 0;

  counters.messagesActiveMasterSlave = 0;
  counters.messagesActiveMasterMaster = 0;
  counters.messagesActiveBroadcast = 0;

  // errors
  counters.errorsTotal = 0;

  counters.errorsPassive = 0;
  counters.errorsPassiveMaster = 0;
  counters.errorsPassiveMasterACK = 0;
  counters.errorsPassiveSlave = 0;
  counters.errorsPassiveSlaveACK = 0;

  counters.errorsReactive = 0;
  counters.errorsReactiveMaster = 0;
  counters.errorsReactiveMasterACK = 0;
  counters.errorsReactiveSlave = 0;
  counters.errorsReactiveSlaveACK = 0;

  counters.errorsActive = 0;
  counters.errorsActiveMaster = 0;
  counters.errorsActiveMasterACK = 0;
  counters.errorsActiveSlave = 0;
  counters.errorsActiveSlaveACK = 0;

  // resets
  counters.resetsTotal = 0;
  counters.resetsPassive = 0;
  counters.resetsPassive = 0;
  counters.resetsActive = 0;

  // requests
  counters.requestsTotal = 0;
  counters.requestsWon = 0;
  counters.requestsLost = 0;
  counters.requestsRetry = 0;
  counters.requestsError = 0;
}

const ebus::Counters &ebus::EbusHandler::getCounters() {
  counters.messagesTotal =
      counters.messagesPassiveMasterSlave +
      counters.messagesPassiveMasterMaster +
      counters.messagesReactiveMasterSlave +
      counters.messagesReactiveMasterMaster +
      counters.messagesReactiveBroadcast + counters.messagesActiveMasterSlave +
      counters.messagesActiveMasterMaster + counters.messagesActiveBroadcast;

  counters.errorsPassive =
      counters.errorsPassiveMaster + counters.errorsPassiveMasterACK +
      counters.errorsPassiveSlave + counters.errorsPassiveSlaveACK;

  counters.errorsReactive =
      counters.errorsReactiveMaster + counters.errorsReactiveMasterACK +
      counters.errorsReactiveSlave + counters.errorsReactiveSlaveACK;

  counters.errorsActive =
      counters.errorsActiveMaster + counters.errorsActiveMasterACK +
      counters.errorsActiveSlave + counters.errorsActiveSlaveACK;

  counters.errorsTotal =
      counters.errorsPassive + counters.errorsReactive + counters.errorsActive;

  counters.resetsTotal = counters.resetsPassive00 + counters.resetsPassive0704 +
                         counters.resetsActive + counters.resetsPassive;

  counters.requestsTotal =
      counters.requestsWon + counters.requestsLost + counters.requestsError;

  return counters;
}

void ebus::EbusHandler::receive(const uint8_t &byte) {
  switch (state) {
    case State::passiveReceiveMaster: {
      if (byte != sym_syn) {
        passiveMaster.push_back(byte);

        if (passiveMaster.size() == 5) passiveMasterDBx = passiveMaster[4];

        // AA >> A9 + 01 || A9 >> A9 + 00
        if (byte == sym_exp) passiveMasterDBx++;

        // size() > ZZ QQ PB SB NN + DBx + CRC
        if (passiveMaster.size() >= 5 + passiveMasterDBx + 1) {
          passiveTelegram.createMaster(passiveMaster);
          if (passiveTelegram.getMasterState() == SEQ_OK) {
            std::vector<uint8_t> response;
            if (passiveTelegram.getType() == Type::broadcast) {
              publishCallback(Message::reactive, Type::broadcast,
                              passiveTelegram.getMaster().to_vector(),
                              &response);
              counters.messagesReactiveBroadcast++;
              resetPassive();
            } else if (passiveMaster[1] == address) {
              publishCallback(Message::reactive, Type::masterMaster,
                              passiveTelegram.getMaster().to_vector(),
                              &response);
              counters.messagesReactiveMasterMaster++;
              state = State::reactiveSendMasterPositiveAcknowledge;
            } else if (passiveMaster[1] == slaveAddress) {
              publishCallback(Message::reactive, Type::masterSlave,
                              passiveTelegram.getMaster().to_vector(),
                              &response);
              counters.messagesReactiveMasterSlave++;
              passiveTelegram.createSlave(response);
              if (passiveTelegram.getSlaveState() == SEQ_OK) {
                passiveSlave = passiveTelegram.getSlave();
                passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
                passiveSlave.extend();
                state = State::reactiveSendMasterPositiveAcknowledge;
              } else {
                counters.errorsReactiveSlave++;
                passiveErrors();
                state = State::releaseBus;
              }
            } else {
              state = State::passiveReceiveMasterAcknowledge;
            }
          } else {
            if (passiveMaster[1] == address ||
                passiveMaster[1] == slaveAddress) {
              counters.errorsReactiveMaster++;
              passiveTelegram.clear();
              passiveMaster.clear();
              passiveMasterDBx = 0;
              state = State::reactiveSendMasterNegativeAcknowledge;
            } else if (passiveTelegram.getType() == Type::masterMaster ||
                       passiveTelegram.getType() == Type::masterSlave) {
              state = State::passiveReceiveMasterAcknowledge;
            } else {
              counters.errorsPassiveMaster++;
              passiveErrors();
            }
          }
        }
      } else {
        if (passiveMaster.size() != 1 && lockCoutner > 0) lockCoutner--;

        passiveErrors();
        activeErrors();

        if (lockCoutner == 0 && readBufferCallback() == 0 && active) {
          activeMaster = activeTelegram.getMaster();
          activeMaster.push_back(activeTelegram.getMasterCRC(), false);
          activeMaster.extend();
          state = State::requestBusFirstTry;
        }
      }
      break;
    }
    case State::passiveReceiveMasterAcknowledge: {
      if (byte == sym_ack) {
        if (passiveTelegram.getType() == Type::masterMaster) {
          publishCallback(Message::passive, Type::masterMaster,
                          passiveTelegram.getMaster().to_vector(),
                          &const_cast<std::vector<uint8_t> &>(
                              (passiveTelegram.getSlave().to_vector())));
          counters.messagesPassiveMasterMaster++;
          resetPassive();
          state = State::passiveReceiveMaster;
        } else {
          state = State::passiveReceiveSlave;
        }
      } else if (byte != sym_syn && !passiveMasterRepeated) {
        passiveMasterRepeated = true;
        passiveTelegram.clear();
        passiveMaster.clear();
        passiveMasterDBx = 0;
        state = State::passiveReceiveMaster;
      } else {
        counters.errorsPassiveMasterACK++;
        passiveErrors();
        state = State::passiveReceiveMaster;
      }
      break;
    }
    case State::passiveReceiveSlave: {
      passiveSlave.push_back(byte);

      if (passiveSlave.size() == 1) passiveSlaveDBx = byte;

      // AA >> A9 + 01 || A9 >> A9 + 00
      if (byte == sym_exp) passiveSlaveDBx++;

      // size() > NN + DBx + CRC
      if (passiveSlave.size() >= 1 + passiveSlaveDBx + 1) {
        passiveTelegram.createSlave(passiveSlave);
        if (passiveTelegram.getSlaveState() != SEQ_OK)
          counters.errorsPassiveSlave++;
        state = State::passiveReceiveSlaveAcknowledge;
      }
      break;
    }
    case State::passiveReceiveSlaveAcknowledge: {
      if (byte == sym_ack) {
        publishCallback(Message::passive, Type::masterSlave,
                        passiveTelegram.getMaster().to_vector(),
                        &const_cast<std::vector<uint8_t> &>(
                            (passiveTelegram.getSlave().to_vector())));
        counters.messagesPassiveMasterSlave++;
        resetPassive();
        state = State::passiveReceiveMaster;
      } else if (byte == sym_nak && !passiveSlaveRepeated) {
        passiveSlaveRepeated = true;
        passiveSlave.clear();
        passiveSlaveDBx = 0;
        state = State::passiveReceiveSlave;
      } else {
        counters.errorsPassiveSlaveACK++;
        passiveErrors();
        state = State::passiveReceiveMaster;
      }
      break;
    }
    case State::reactiveSendMasterPositiveAcknowledge: {
      if (passiveTelegram.getType() == Type::masterMaster) {
        resetPassive();
        state = State::passiveReceiveMaster;
      } else {
        state = State::reactiveSendSlave;
      }
      break;
    }
    case State::reactiveSendMasterNegativeAcknowledge: {
      state = State::passiveReceiveMaster;
      if (!passiveMasterRepeated) {
        passiveMasterRepeated = true;
      } else {
        counters.errorsReactiveMasterACK++;
        passiveErrors();
      }
      break;
    }
    case State::reactiveSendSlave: {
      passiveSlaveIndex++;
      if (passiveSlaveIndex >= passiveSlave.size())
        state = State::reactiveReceiveSlaveAcknowledge;
      break;
    }
    case State::reactiveReceiveSlaveAcknowledge: {
      if (byte == sym_nak && !passiveSlaveRepeated) {
        passiveSlaveRepeated = true;
        passiveSlaveIndex = 0;
        state = State::reactiveSendSlave;
      } else {
        if (byte == sym_nak) {
          counters.errorsReactiveSlaveACK++;
          passiveErrors();
        } else {
          resetPassive();
        }
        state = State::passiveReceiveMaster;
      }
      break;
    }
    case State::requestBusFirstTry: {
      if (byte != address) {
        if ((byte & 0x0f) == (address & 0x0f)) {
          state = State::requestBusPriorityRetry;
        } else {
          counters.requestsLost++;
          passiveMaster.push_back(byte);
          active = false;
          activeTelegram.clear();
          activeMaster.clear();
          state = State::passiveReceiveMaster;
        }
      } else {
        counters.requestsWon++;
        activeMasterIndex = 1;
        state = State::activeSendMaster;
      }
      break;
    }
    case State::requestBusPriorityRetry: {
      if (byte != sym_syn) {
        counters.requestsError++;
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
        state = State::passiveReceiveMaster;
      } else {
        counters.requestsRetry++;
        state = State::requestBusSecondTry;
      }
      break;
    }
    case State::requestBusSecondTry: {
      if (byte != address) {
        counters.requestsLost++;
        passiveMaster.push_back(byte);
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
        state = State::passiveReceiveMaster;
      } else {
        counters.requestsWon++;
        activeMasterIndex = 1;
        state = State::activeSendMaster;
      }
      break;
    }
    case State::activeSendMaster: {
      activeMasterIndex++;
      if (activeMasterIndex >= activeMaster.size()) {
        if (activeTelegram.getType() == Type::broadcast) {
          publishCallback(Message::active, Type::broadcast,
                          activeTelegram.getMaster().to_vector(),
                          &const_cast<std::vector<uint8_t> &>(
                              (activeTelegram.getSlave().to_vector())));
          counters.messagesActiveBroadcast++;
          resetActive();
          state = State::releaseBus;
        } else {
          state = State::activeReceiveMasterAcknowledge;
        }
      }
      break;
    }
    case State::activeReceiveMasterAcknowledge: {
      if (byte == sym_ack) {
        if (activeTelegram.getType() == Type::masterMaster) {
          publishCallback(Message::active, Type::masterMaster,
                          activeTelegram.getMaster().to_vector(),
                          &const_cast<std::vector<uint8_t> &>(
                              (activeTelegram.getSlave().to_vector())));
          counters.messagesActiveMasterMaster++;
          resetActive();
          state = State::releaseBus;
        } else {
          state = State::activeReceiveSlave;
        }
      } else if (!activeMasterRepeated) {
        activeMasterRepeated = true;
        activeMasterIndex = 0;
        state = State::activeSendMaster;
      } else {
        counters.errorsActiveMasterACK++;
        activeErrors();
        state = State::releaseBus;
      }
      break;
    }
    case State::activeReceiveSlave: {
      activeSlave.push_back(byte);

      if (activeSlave.size() == 1) activeSlaveDBx = byte;

      // AA >> A9 + 01 || A9 >> A9 + 00
      if (byte == sym_exp) activeSlaveDBx++;

      // size() > NN + DBx + CRC
      if (activeSlave.size() >= 1 + activeSlaveDBx + 1) {
        activeTelegram.createSlave(activeSlave);
        if (activeTelegram.getSlaveState() == SEQ_OK) {
          state = State::activeSendSlavePositiveAcknowledge;
        } else {
          counters.errorsActiveSlave++;
          activeSlave.clear();
          activeSlaveDBx = 0;
          state = State::activeSendSlaveNegativeAcknowledge;
        }
      }
      break;
    }
    case State::activeSendSlavePositiveAcknowledge: {
      publishCallback(Message::active, Type::masterSlave,
                      activeTelegram.getMaster().to_vector(),
                      &const_cast<std::vector<uint8_t> &>(
                          (activeTelegram.getSlave().to_vector())));
      counters.messagesActiveMasterSlave++;
      resetActive();
      state = State::releaseBus;
      break;
    }
    case State::activeSendSlaveNegativeAcknowledge: {
      if (!activeSlaveRepeated) {
        activeSlaveRepeated = true;
        state = State::activeReceiveSlave;
      } else {
        counters.errorsActiveSlaveACK++;
        activeErrors();
        state = State::releaseBus;
      }
      break;
    }
    case State::releaseBus: {
      state = State::passiveReceiveMaster;
      break;
    }
  }
}

void ebus::EbusHandler::send() {
  switch (state) {
    case State::passiveReceiveMaster: {
      break;
    }
    case State::passiveReceiveMasterAcknowledge: {
      break;
    }
    case State::passiveReceiveSlave: {
      break;
    }
    case State::passiveReceiveSlaveAcknowledge: {
      break;
    }
    case State::reactiveSendMasterPositiveAcknowledge: {
      writeCallback(sym_ack);
      break;
    }
    case State::reactiveSendMasterNegativeAcknowledge: {
      writeCallback(sym_nak);
      break;
    }
    case State::reactiveSendSlave: {
      writeCallback(passiveSlave[passiveSlaveIndex]);
      break;
    }
    case State::reactiveReceiveSlaveAcknowledge: {
      break;
    }
    case State::requestBusFirstTry: {
      writeCallback(address);
      break;
    }
    case State::requestBusPriorityRetry: {
      break;
    }
    case State::requestBusSecondTry: {
      writeCallback(address);
      break;
    }
    case State::activeSendMaster: {
      writeCallback(activeMaster[activeMasterIndex]);
      break;
    }
    case State::activeReceiveMasterAcknowledge: {
      break;
    }
    case State::activeReceiveSlave: {
      break;
    }
    case State::activeSendSlavePositiveAcknowledge: {
      writeCallback(sym_ack);
      break;
    }
    case State::activeSendSlaveNegativeAcknowledge: {
      writeCallback(sym_nak);
      break;
    }
    case State::releaseBus: {
      writeCallback(sym_syn);
      break;
    }
  }
}

void ebus::EbusHandler::passiveErrors() {
  if (passiveMaster.size() > 0 || passiveMasterDBx > 0 ||
      passiveMasterRepeated || passiveSlave.size() > 0 || passiveSlaveDBx > 0 ||
      passiveSlaveIndex > 0 || passiveSlaveRepeated) {
    if (errorCallback != nullptr) {
      std::ostringstream ostr;
      ostr << "passive";
      ostr << " | master: '" << passiveMaster.to_string();
      ostr << "' DBx: " << passiveMasterDBx;
      ostr << " repeated: " << (passiveMasterRepeated ? "true" : "false");
      ostr << " | slave: '" << passiveSlave.to_string();
      ostr << "' DBx: " << passiveSlaveDBx;
      ostr << " index: " << passiveSlaveIndex;
      ostr << " repeated: " << (passiveSlaveRepeated ? "true" : "false");
      errorCallback(ostr.str());
    }

    if (passiveMaster.size() == 1 && passiveMaster[0] == 0x00)
      counters.resetsPassive00++;
    else if (passiveMaster.size() == 6 && passiveMaster[2] == 0x07 &&
             passiveMaster[3] == 0x04)
      counters.resetsPassive0704++;
    else
      counters.resetsPassive++;

    resetPassive();
  }
}

void ebus::EbusHandler::activeErrors() {
  if (activeMaster.size() > 0 || activeMasterIndex > 0 ||
      activeMasterRepeated || activeSlave.size() > 0 || activeSlaveDBx > 0 ||
      activeSlaveRepeated) {
    if (errorCallback != nullptr) {
      std::ostringstream ostr;
      ostr << "active";
      ostr << " | master: '" << activeMaster.to_string();
      ostr << "' index: " << activeMasterIndex;
      ostr << " repeated: " << (activeMasterRepeated ? "true" : "false");
      ostr << " | slave: '" << activeSlave.to_string();
      ostr << "' DBx: " << activeSlaveDBx;
      ostr << " repeated: " << (activeSlaveRepeated ? "true" : "false");
      errorCallback(ostr.str());
    }

    counters.resetsActive++;
    resetActive();
  }
}

void ebus::EbusHandler::resetPassive() {
  passiveTelegram.clear();

  passiveMaster.clear();
  passiveMasterDBx = 0;
  passiveMasterRepeated = false;

  passiveSlave.clear();
  passiveSlaveDBx = 0;
  passiveSlaveIndex = 0;
  passiveSlaveRepeated = false;
}

void ebus::EbusHandler::resetActive() {
  lockCoutner = maxLockCounter;

  active = false;
  activeTelegram.clear();

  activeMaster.clear();
  activeMasterIndex = 0;
  activeMasterRepeated = false;

  activeSlave.clear();
  activeSlaveDBx = 0;
  activeSlaveRepeated = false;
}
