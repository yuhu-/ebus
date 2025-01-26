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
    const uint8_t source, std::function<bool()> busReadyFunction,
    std::function<void(const uint8_t byte)> busWriteFunction,
    std::function<void(const std::vector<uint8_t> master,
                       const std::vector<uint8_t> slave)>
        activeFunction,
    std::function<void(const std::vector<uint8_t> master,
                       const std::vector<uint8_t> slave)>
        passiveFunction,
    std::function<void(const std::vector<uint8_t> master,
                       std::vector<uint8_t> *const slave)>
        reactiveFunction) {
  setAddress(source);
  busReadyCallback = busReadyFunction;
  busWriteCallback = busWriteFunction;
  activeCallback = activeFunction;
  passiveCallback = passiveFunction;
  reactiveCallback = reactiveFunction;
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

void ebus::EbusHandler::setExternalBusRequest(const bool external) {
  this->external = external;
}

ebus::State ebus::EbusHandler::getState() const { return state; }

bool ebus::EbusHandler::isActive() const { return active; }

void ebus::EbusHandler::reset() {
  state = State::passiveReceiveMaster;
  write = false;
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

void ebus::EbusHandler::wonExternalBusRequest(const bool won) {
  if (won) {
    resetPassive();

    activeMaster = activeTelegram.getMaster();
    activeMaster.push_back(activeTelegram.getMasterCRC(), false);
    activeMaster.extend();

    state = State::activeSendMaster;
    activeMasterSendIndex = 1;
    activeMasterReceiveIndex = 1;

    send();
  } else {
    resetActive();
  }
}

void ebus::EbusHandler::run(const uint8_t &byte) {
  receive(byte);
  send();
}

void ebus::EbusHandler::resetCounters() {
  // messages
  counters.messagesTotal = 0;

  counters.messagesPassiveMS = 0;
  counters.messagesPassiveMM = 0;

  counters.messagesReactiveMS = 0;
  counters.messagesReactiveMM = 0;
  counters.messagesReactiveBC = 0;

  counters.messagesActiveMS = 0;
  counters.messagesActiveMM = 0;
  counters.messagesActiveBC = 0;

  // errors
  counters.errorsTotal = 0;

  counters.errorsPassive = 0;
  counters.errorsPassiveMaster = 0;
  counters.errorsPassiveMasterACK = 0;
  counters.errorsPassiveSlave = 0;
  counters.errorsPassiveSlaveACK = 0;
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
      counters.messagesPassiveMS + counters.messagesPassiveMM +
      counters.messagesReactiveMS + counters.messagesReactiveMM +
      counters.messagesReactiveBC + counters.messagesActiveMS +
      counters.messagesActiveMM + counters.messagesActiveBC;

  counters.errorsPassive =
      counters.errorsPassiveMaster + counters.errorsPassiveMasterACK +
      counters.errorsPassiveSlave + counters.errorsPassiveSlaveACK +
      counters.errorsReactiveSlave + counters.errorsReactiveSlaveACK;

  counters.errorsActive =
      counters.errorsActiveMaster + counters.errorsActiveMasterACK +
      counters.errorsActiveSlave + counters.errorsActiveSlaveACK;

  counters.errorsTotal = counters.errorsPassive + counters.errorsActive;

  counters.resetsTotal =
      counters.resetsPassive00 + counters.resetsPassive + counters.resetsActive;

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
            if (passiveTelegram.getType() == Type::BC) {
              reactiveCallback(passiveTelegram.getMaster().to_vector(),
                               nullptr);
              counters.messagesReactiveBC++;
              resetPassive();
            } else if (passiveMaster[1] == address ||
                       passiveMaster[1] == slaveAddress) {
              state = State::reactiveSendMasterPositiveAcknowledge;
              write = true;

              if (passiveTelegram.getType() == Type::MM) {
                reactiveCallback(passiveTelegram.getMaster().to_vector(),
                                 nullptr);
                counters.messagesReactiveMM++;
              } else if (passiveTelegram.getType() == Type::MS) {
                std::vector<uint8_t> response;
                reactiveCallback(passiveTelegram.getMaster().to_vector(),
                                 &response);
                counters.messagesReactiveMS++;
                passiveTelegram.createSlave(response);
                if (passiveTelegram.getSlaveState() == SEQ_OK) {
                  passiveSlave = passiveTelegram.getSlave();
                  passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
                  passiveSlave.extend();
                } else {
                  counters.errorsReactiveSlave++;
                  state = State::releaseBus;
                  write = true;
                  resetPassive();
                }
              }
            } else {
              state = State::passiveReceiveMasterAcknowledge;
            }
          } else {
            if (passiveMaster[1] == address ||
                passiveMaster[1] == slaveAddress) {
              state = State::reactiveSendMasterNegativeAcknowledge;
              write = true;
              passiveTelegram.clear();
              passiveMaster.clear();
              passiveMasterDBx = 0;
            } else if (passiveTelegram.getType() == Type::MM ||
                       passiveTelegram.getType() == Type::MS) {
              state = State::passiveReceiveMasterAcknowledge;
            } else {
              counters.errorsPassiveMaster++;
              resetPassive();
            }
          }
        }
      } else {
        if (passiveMaster.size() != 1 && lockCoutner > 0) lockCoutner--;

        if (passiveMaster.size() > 0 || passiveSlave.size() > 0 ||
            passiveMasterDBx > 0 || passiveSlaveDBx > 0 ||
            passiveSlaveSendIndex > 0 || passiveSlaveReceiveIndex > 0 ||
            passiveMasterRepeated || passiveSlaveRepeated) {
          if (errorCallback != nullptr) {
            std::ostringstream ostr;
            ostr << "passive";
            ostr << " M '" << passiveMaster.to_string();
            ostr << "' S '" << passiveSlave.to_string();
            ostr << "' MDBx " << passiveMasterDBx;
            ostr << " SDBx " << passiveSlaveDBx;
            ostr << " SSI " << passiveSlaveSendIndex;
            ostr << " SRI " << passiveSlaveReceiveIndex;
            ostr << " MR " << (passiveMasterRepeated ? "true" : "false");
            ostr << " SR " << (passiveSlaveRepeated ? "true" : "false");
            errorCallback(ostr.str());
          }

          if (passiveMaster.size() == 1 && passiveMaster[0] == 0x00)
            counters.resetsPassive00++;
          else
            counters.resetsPassive++;

          resetPassive();
        }

        if (activeMaster.size() > 0 || activeSlave.size() > 0 ||
            activeSlaveDBx > 0 || activeMasterSendIndex > 0 ||
            activeMasterReceiveIndex > 0 || activeMasterRepeated ||
            activeSlaveRepeated) {
          if (errorCallback != nullptr) {
            std::ostringstream ostr;
            ostr << "active";
            ostr << " M '" << activeMaster.to_string();
            ostr << "' S '" << activeSlave.to_string();
            ostr << " SDBx " << activeSlaveDBx;
            ostr << " SSI " << activeMasterSendIndex;
            ostr << " SRI " << activeMasterReceiveIndex;
            ostr << " MR " << (activeMasterRepeated ? "true" : "false");
            ostr << " SR " << (activeSlaveRepeated ? "true" : "false");
            errorCallback(ostr.str());
          }

          counters.resetsActive++;
          resetActive();
        }

        if (!external) {
          if (lockCoutner == 0 && active) {
            activeMaster = activeTelegram.getMaster();
            activeMaster.push_back(activeTelegram.getMasterCRC(), false);
            activeMaster.extend();
            state = State::requestBusFirstTry;
            write = true;
          }
        }
      }
      break;
    }
    case State::passiveReceiveMasterAcknowledge: {
      if (byte == sym_ack) {
        if (passiveTelegram.getType() == Type::MM) {
          passiveCallback(passiveTelegram.getMaster().to_vector(),
                          passiveTelegram.getSlave().to_vector());
          counters.messagesPassiveMM++;
          state = State::passiveReceiveMaster;
          resetPassive();
        } else {
          state = State::passiveReceiveSlave;
        }
      } else if (!passiveMasterRepeated) {
        passiveMasterRepeated = true;
        state = State::passiveReceiveMaster;
        passiveTelegram.clear();
        passiveMaster.clear();
        passiveMasterDBx = 0;
      } else {
        counters.errorsPassiveMasterACK++;
        state = State::passiveReceiveMaster;
        resetPassive();
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
        passiveCallback(passiveTelegram.getMaster().to_vector(),
                        passiveTelegram.getSlave().to_vector());
        counters.messagesPassiveMS++;
        state = State::passiveReceiveMaster;
        resetPassive();
      } else if (byte == sym_nak && !passiveSlaveRepeated) {
        passiveSlaveRepeated = true;
        state = State::passiveReceiveSlave;
        passiveSlave.clear();
        passiveSlaveDBx = 0;
      } else {
        counters.errorsPassiveSlaveACK++;
        state = State::passiveReceiveMaster;
        resetPassive();
      }
      break;
    }
    case State::reactiveSendMasterPositiveAcknowledge: {
      if (passiveTelegram.getType() == Type::MM) {
        state = State::releaseBus;
        write = true;
        resetPassive();
      } else {
        state = State::reactiveSendSlave;
      }
      break;
    }
    case State::reactiveSendMasterNegativeAcknowledge: {
      state = State::passiveReceiveMaster;
      break;
    }
    case State::reactiveSendSlave: {
      passiveSlaveReceiveIndex++;
      if (passiveSlaveReceiveIndex >= passiveSlave.size())
        state = State::reactiveReceiveSlaveAcknowledge;
      break;
    }
    case State::reactiveReceiveSlaveAcknowledge: {
      if (byte == sym_nak && !passiveSlaveRepeated) {
        passiveSlaveRepeated = true;
        state = State::reactiveSendSlave;
        passiveSlaveSendIndex = 0;
        passiveSlaveReceiveIndex = 0;
      } else {
        if (byte == sym_nak) counters.errorsReactiveSlaveACK++;
        state = State::releaseBus;
        write = true;
        resetPassive();
      }
      break;
    }
    case State::requestBusFirstTry: {
      if (byte != address) {
        if ((byte & 0x0f) == (address & 0x0f)) {
          state = State::requestBusPriorityRetry;
        } else {
          counters.requestsLost++;
          state = State::passiveReceiveMaster;
          passiveMaster.push_back(byte);
          active = false;
          activeTelegram.clear();
          activeMaster.clear();
        }
      } else {
        counters.requestsWon++;
        state = State::activeSendMaster;
        activeMasterSendIndex = 1;
        activeMasterReceiveIndex = 1;
      }
      break;
    }
    case State::requestBusPriorityRetry: {
      if (byte != sym_syn) {
        counters.requestsError++;
        state = State::passiveReceiveMaster;
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
      } else {
        counters.requestsRetry++;
        state = State::requestBusSecondTry;
        write = true;
      }
      break;
    }
    case State::requestBusSecondTry: {
      if (byte != address) {
        counters.requestsLost++;
        state = State::passiveReceiveMaster;
        passiveMaster.push_back(byte);
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
      } else {
        counters.requestsWon++;
        state = State::activeSendMaster;
        activeMasterSendIndex = 1;
        activeMasterReceiveIndex = 1;
      }
      break;
    }
    case State::activeSendMaster: {
      activeMasterReceiveIndex++;
      if (activeMasterReceiveIndex >= activeMaster.size()) {
        if (activeTelegram.getType() == Type::BC) {
          activeCallback(activeTelegram.getMaster().to_vector(),
                         activeTelegram.getSlave().to_vector());
          counters.messagesActiveBC++;
          state = State::releaseBus;
          write = true;
          lockCoutner = maxLockCounter;
          resetActive();
        } else {
          state = State::activeReceiveMasterAcknowledge;
        }
      }
      break;
    }
    case State::activeReceiveMasterAcknowledge: {
      if (byte == sym_ack) {
        if (activeTelegram.getType() == Type::MM) {
          activeCallback(activeTelegram.getMaster().to_vector(),
                         activeTelegram.getSlave().to_vector());
          counters.messagesActiveMM++;
          state = State::releaseBus;
          write = true;
          lockCoutner = maxLockCounter;
          resetActive();
        } else {
          state = State::activeReceiveSlave;
        }
      } else if (!activeMasterRepeated) {
        activeMasterRepeated = true;
        state = State::activeSendMaster;
        activeMasterSendIndex = 0;
        activeMasterReceiveIndex = 0;
      } else {
        counters.errorsActiveMasterACK++;
        state = State::releaseBus;
        write = true;
        lockCoutner = maxLockCounter;
        resetActive();
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
          write = true;
        } else {
          counters.errorsActiveSlave++;
          state = State::activeSendSlaveNegativeAcknowledge;
          write = true;
          activeSlave.clear();
          activeSlaveDBx = 0;
        }
      }
      break;
    }
    case State::activeSendSlavePositiveAcknowledge: {
      activeCallback(activeTelegram.getMaster().to_vector(),
                     activeTelegram.getSlave().to_vector());
      counters.messagesActiveMS++;
      state = State::releaseBus;
      write = true;
      lockCoutner = maxLockCounter;
      resetActive();
      break;
    }
    case State::activeSendSlaveNegativeAcknowledge: {
      if (!activeSlaveRepeated) {
        activeSlaveRepeated = true;
        state = State::activeReceiveSlave;
        write = true;
      } else {
        counters.errorsActiveSlaveACK++;
        state = State::releaseBus;
        write = true;
        lockCoutner = maxLockCounter;
        resetActive();
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
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(sym_ack);
      }
      break;
    }
    case State::reactiveSendMasterNegativeAcknowledge: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(sym_nak);
      }
      break;
    }
    case State::reactiveSendSlave: {
      if (busReadyCallback() &&
          passiveSlaveSendIndex == passiveSlaveReceiveIndex) {
        busWriteCallback(passiveSlave[passiveSlaveSendIndex]);
        passiveSlaveSendIndex++;
      }
      break;
    }
    case State::reactiveReceiveSlaveAcknowledge: {
      break;
    }
    case State::requestBusFirstTry: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(address);
      }
      break;
    }
    case State::requestBusPriorityRetry: {
      break;
    }
    case State::requestBusSecondTry: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(address);
      }
      break;
    }
    case State::activeSendMaster: {
      if (busReadyCallback() &&
          activeMasterSendIndex == activeMasterReceiveIndex) {
        busWriteCallback(activeMaster[activeMasterSendIndex]);
        activeMasterSendIndex++;
      }
      break;
    }
    case State::activeReceiveMasterAcknowledge: {
      break;
    }
    case State::activeReceiveSlave: {
      break;
    }
    case State::activeSendSlavePositiveAcknowledge: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(sym_ack);
      }
      break;
    }
    case State::activeSendSlaveNegativeAcknowledge: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(sym_nak);
      }
      break;
    }
    case State::releaseBus: {
      if (busReadyCallback() && write) {
        write = false;
        busWriteCallback(sym_syn);
      }
      break;
    }
  }
}

void ebus::EbusHandler::resetPassive() {
  passiveTelegram.clear();

  passiveMaster.clear();
  passiveMasterDBx = 0;
  passiveMasterRepeated = false;

  passiveSlave.clear();
  passiveSlaveDBx = 0;
  passiveSlaveSendIndex = 0;
  passiveSlaveReceiveIndex = 0;
  passiveSlaveRepeated = false;
}

void ebus::EbusHandler::resetActive() {
  active = false;
  activeTelegram.clear();

  activeMaster.clear();
  activeMasterSendIndex = 0;
  activeMasterReceiveIndex = 0;
  activeMasterRepeated = false;

  activeSlave.clear();
  activeSlaveDBx = 0;
  activeSlaveRepeated = false;
}
