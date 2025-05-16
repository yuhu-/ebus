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

#include <utility>
#include <vector>

#include "Telegram.h"

ebus::EbusHandler::EbusHandler(const uint8_t source) { setAddress(source); }

void ebus::EbusHandler::onWrite(ebus::OnWriteCallback callback) {
  onWriteCallback = callback;
}

void ebus::EbusHandler::isDataAvailable(
    ebus::IsDataAvailableCallback callback) {
  isDataAvailableCallback = callback;
}

void ebus::EbusHandler::onTelegram(ebus::OnTelegramCallback callback) {
  onTelegramCallback = callback;
}

void ebus::EbusHandler::onError(ebus::OnErrorCallback callback) {
  onErrorCallback = callback;
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

  lockCounter = maxLockCounter;
}

ebus::State ebus::EbusHandler::getState() const { return state; }

bool ebus::EbusHandler::isActive() const { return active; }

void ebus::EbusHandler::reset() {
  state = State::passiveReceiveMaster;
  resetActive();
  resetPassive();
}

bool ebus::EbusHandler::enque(const std::vector<uint8_t> &message) {
  if (message.empty()) return false;

  active = false;
  if (onWriteCallback != nullptr) {
    activeTelegram.createMaster(address, message);
    if (activeTelegram.getMasterState() == SEQ_OK)
      active = true;
    else
      counters.errorsActiveMaster++;
  }
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
  counters.resetsPassive00 = 0;
  counters.resetsPassive0704 = 0;
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
              if (onTelegramCallback != nullptr) {
                onTelegramCallback(Message::reactive, Type::broadcast,
                                   passiveTelegram.getMaster().to_vector(),
                                   &response);
              }
              counters.messagesReactiveBroadcast++;
              resetPassive();
            } else if (passiveMaster[1] == address) {
              if (onTelegramCallback != nullptr) {
                onTelegramCallback(Message::reactive, Type::masterMaster,
                                   passiveTelegram.getMaster().to_vector(),
                                   &response);
              }
              counters.messagesReactiveMasterMaster++;
              state = State::reactiveSendMasterPositiveAcknowledge;
            } else if (passiveMaster[1] == slaveAddress) {
              if (onTelegramCallback != nullptr) {
                onTelegramCallback(Message::reactive, Type::masterSlave,
                                   passiveTelegram.getMaster().to_vector(),
                                   &response);
              }
              counters.messagesReactiveMasterSlave++;
              passiveTelegram.createSlave(response);
              if (passiveTelegram.getSlaveState() == SEQ_OK) {
                passiveSlave = passiveTelegram.getSlave();
                passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
                passiveSlave.extend();
                state = State::reactiveSendMasterPositiveAcknowledge;
              } else {
                counters.errorsReactiveSlave++;
                onPassiveErrors();
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
              onPassiveErrors();
            }
          }
        }
      } else {
        if (passiveMaster.size() != 1 && lockCounter > 0) lockCounter--;

        onPassiveErrors();
        onActiveErrors();

        int available = 0;
        if (isDataAvailableCallback != nullptr)
          available = isDataAvailableCallback();

        if (lockCounter == 0 && available == 0 && active) {
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
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(Message::passive, Type::masterMaster,
                               passiveTelegram.getMaster().to_vector(),
                               &const_cast<std::vector<uint8_t> &>(
                                   (passiveTelegram.getSlave().to_vector())));
          }
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
        onPassiveErrors();
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
        if (onTelegramCallback != nullptr) {
          onTelegramCallback(Message::passive, Type::masterSlave,
                             passiveTelegram.getMaster().to_vector(),
                             &const_cast<std::vector<uint8_t> &>(
                                 (passiveTelegram.getSlave().to_vector())));
        }
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
        onPassiveErrors();
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
        onPassiveErrors();
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
          onPassiveErrors();
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
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(Message::active, Type::broadcast,
                               activeTelegram.getMaster().to_vector(),
                               &const_cast<std::vector<uint8_t> &>(
                                   (activeTelegram.getSlave().to_vector())));
          }
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
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(Message::active, Type::masterMaster,
                               activeTelegram.getMaster().to_vector(),
                               &const_cast<std::vector<uint8_t> &>(
                                   (activeTelegram.getSlave().to_vector())));
          }
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
        onActiveErrors();
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
      if (onTelegramCallback != nullptr) {
        onTelegramCallback(Message::active, Type::masterSlave,
                           activeTelegram.getMaster().to_vector(),
                           &const_cast<std::vector<uint8_t> &>(
                               (activeTelegram.getSlave().to_vector())));
      }
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
        onActiveErrors();
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
      onWriteCallback(sym_ack);
      break;
    }
    case State::reactiveSendMasterNegativeAcknowledge: {
      onWriteCallback(sym_nak);
      break;
    }
    case State::reactiveSendSlave: {
      onWriteCallback(passiveSlave[passiveSlaveIndex]);
      break;
    }
    case State::reactiveReceiveSlaveAcknowledge: {
      break;
    }
    case State::requestBusFirstTry: {
      onWriteCallback(address);
      break;
    }
    case State::requestBusPriorityRetry: {
      break;
    }
    case State::requestBusSecondTry: {
      onWriteCallback(address);
      break;
    }
    case State::activeSendMaster: {
      onWriteCallback(activeMaster[activeMasterIndex]);
      break;
    }
    case State::activeReceiveMasterAcknowledge: {
      break;
    }
    case State::activeReceiveSlave: {
      break;
    }
    case State::activeSendSlavePositiveAcknowledge: {
      onWriteCallback(sym_ack);
      break;
    }
    case State::activeSendSlaveNegativeAcknowledge: {
      onWriteCallback(sym_nak);
      break;
    }
    case State::releaseBus: {
      onWriteCallback(sym_syn);
      break;
    }
  }
}

void ebus::EbusHandler::onPassiveErrors() {
  if (passiveMaster.size() > 0 || passiveMasterDBx > 0 ||
      passiveMasterRepeated || passiveSlave.size() > 0 || passiveSlaveDBx > 0 ||
      passiveSlaveIndex > 0 || passiveSlaveRepeated) {
    if (onErrorCallback != nullptr) {
      std::string errorMessage =
          "passive | master: '" + passiveMaster.to_string() +
          "' DBx: " + std::to_string(passiveMasterDBx) +
          " repeated: " + (passiveMasterRepeated ? "true" : "false") +
          " | slave: '" + passiveSlave.to_string() +
          "' DBx: " + std::to_string(passiveSlaveDBx) +
          " index: " + std::to_string(passiveSlaveIndex) +
          " repeated: " + (passiveSlaveRepeated ? "true" : "false");
      onErrorCallback(errorMessage);
    }

    if (passiveMaster.size() == 1 && passiveMaster[0] == 0x00)
      counters.resetsPassive00++;
    else if (passiveMaster.size() == 6 && passiveMaster[2] == 0x07 &&
             passiveMaster[3] == 0x04)
      counters.resetsPassive0704++;
    else if (passiveMaster.size() >= 4 && passiveMaster.size() == 6 &&
             passiveMaster[2] == 0x07 && passiveMaster[3] == 0x04)

      resetPassive();
  }
}

void ebus::EbusHandler::onActiveErrors() {
  if (activeMaster.size() > 0 || activeMasterIndex > 0 ||
      activeMasterRepeated || activeSlave.size() > 0 || activeSlaveDBx > 0 ||
      activeSlaveRepeated) {
    if (onErrorCallback != nullptr) {
      std::string errorMessage =
          "active | master: '" + activeMaster.to_string() +
          "' index: " + std::to_string(activeMasterIndex) +
          " repeated: " + (activeMasterRepeated ? "true" : "false") +
          " | slave: '" + activeSlave.to_string() +
          "' DBx: " + std::to_string(activeSlaveDBx) +
          " repeated: " + (activeSlaveRepeated ? "true" : "false");
      onErrorCallback(errorMessage);
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
  lockCounter = maxLockCounter;

  active = false;
  activeTelegram.clear();

  activeMaster.clear();
  activeMasterIndex = 0;
  activeMasterRepeated = false;

  activeSlave.clear();
  activeSlaveDBx = 0;
  activeSlaveRepeated = false;
}
