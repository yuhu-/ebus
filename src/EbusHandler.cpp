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

void ebus::EbusHandler::setTraceCallback(
    std::function<void(const char *)> traceFunction) {
  traceCallback = traceFunction;
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
  write = false;
  resetActive();
  resetPassive();
}

bool ebus::EbusHandler::enque(const std::vector<uint8_t> &message) {
  if (!active) {
    activeTelegram.createMaster(address, message);
    if (activeTelegram.getMasterState() == SEQ_OK) {
      active = true;
    }
  }
  return active;
}

void ebus::EbusHandler::run(const uint8_t &byte) {
  receive(byte);
  send();
}

void ebus::EbusHandler::resetCounters() {
  counters.total = 0;

  counters.passive = 0;
  counters.passivePercent = 0;

  counters.passiveMS = 0;
  counters.passiveMM = 0;
  counters.passiveBC = 0;

  counters.passiveMSAtMe = 0;
  counters.passiveMMAtMe = 0;

  counters.active = 0;
  counters.activePercent = 0;

  counters.activeMS = 0;
  counters.activeMM = 0;
  counters.activeBC = 0;

  counters.failure = 0;
  counters.failurePercent = 0;

  counters.requestTotal = 0;

  counters.requestWon = 0;
  counters.requestWonPercent = 0;
  counters.requestWon1 = 0;
  counters.requestWon2 = 0;
  counters.requestRetry = 0;

  counters.requestLost = 0;
  counters.requestLostPercent = 0;
  counters.requestLost1 = 0;
  counters.requestLost2 = 0;

  counters.requestError = 0;
  counters.requestErrorPercent = 0;
}

const ebus::Counters &ebus::EbusHandler::getCounters() {
  counters.passive = counters.passiveMS + counters.passiveMM +
                     counters.passiveBC + counters.passiveMMAtMe +
                     counters.passiveMSAtMe;

  counters.active = counters.activeMS + counters.activeMM + counters.activeBC;

  counters.total = counters.passive + counters.active + counters.failure;

  counters.passivePercent =
      counters.passive / static_cast<float>(counters.total) * 100.0f;

  counters.activePercent =
      counters.active / static_cast<float>(counters.total) * 100.0f;

  counters.failurePercent =
      counters.failure / static_cast<float>(counters.total) * 100.0f;

  counters.requestWon = counters.requestWon1 + counters.requestWon2;
  counters.requestLost = counters.requestLost1 + counters.requestLost2;
  counters.requestTotal =
      counters.requestWon + counters.requestLost + counters.requestError;

  counters.requestWonPercent =
      counters.requestWon / static_cast<float>(counters.requestTotal) * 100.0f;

  counters.requestLostPercent =
      counters.requestLost / static_cast<float>(counters.requestTotal) * 100.0f;

  counters.requestErrorPercent = counters.requestError /
                                 static_cast<float>(counters.requestTotal) *
                                 100.0f;

  return counters;
}

void ebus::EbusHandler::receive(const uint8_t &byte) {
  if (traceCallback != nullptr) traceCallback(stateString(state));

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
              counters.passiveBC++;
              resetPassive();
            } else if (passiveMaster[1] == address ||
                       passiveMaster[1] == slaveAddress) {
              state = State::reactiveSendMasterPositiveAcknowledge;
              write = true;

              if (passiveTelegram.getType() == Type::MM) {
                reactiveCallback(passiveTelegram.getMaster().to_vector(),
                                 nullptr);
                counters.passiveMMAtMe++;
              } else if (passiveTelegram.getType() == Type::MS) {
                std::vector<uint8_t> response;
                reactiveCallback(passiveTelegram.getMaster().to_vector(),
                                 &response);
                counters.passiveMSAtMe++;
                passiveTelegram.createSlave(response);
                if (passiveTelegram.getSlaveState() == SEQ_OK) {
                  passiveSlave = passiveTelegram.getSlave();
                  passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
                  passiveSlave.extend();
                } else {
                  state = State::releaseBus;
                  write = true;
                  resetPassive();
                }
              }
            } else {
              state = State::passiveReceiveMasterAcknowledge;
            }
          } else {
            if (passiveTelegram.getType() == Type::MM ||
                passiveTelegram.getType() == Type::MS) {
              state = State::reactiveSendMasterNegativeAcknowledge;
              write = true;
              passiveTelegram.clear();
              passiveMaster.clear();
              passiveMasterDBx = 0;
            } else {
              counters.failure++;
            }
          }
        }

      } else {
        if (passiveMaster.size() != 1 && lockCoutner > 0) lockCoutner--;

        if (passiveMaster.size() > 0 || passiveSlave.size() > 0 ||
            passiveMasterDBx > 0 || passiveSlaveDBx > 0 ||
            passiveSlaveSendIndex > 0 || passiveSlaveReceiveIndex > 0 ||
            passiveSlaveRepeated) {
          if (traceCallback != nullptr) traceCallback("passive data reseted");
          resetPassive();
          counters.failure++;
        }

        if (activeMaster.size() > 0 || activeSlave.size() > 0 ||
            activeSlaveDBx > 0 || activeMasterSendIndex > 0 ||
            activeMasterReceiveIndex > 0 || activeMasterRepeated ||
            activeSlaveRepeated) {
          if (!active) {
            if (traceCallback != nullptr) traceCallback("active data reseted");
            resetActive();
            counters.failure++;
          }
        }

        // starting active telegram
        if (lockCoutner == 0 && active) {
          activeMaster = activeTelegram.getMaster();
          activeMaster.push_back(activeTelegram.getMasterCRC(), false);
          activeMaster.extend();
          state = State::requestBusFirstTry;
          write = true;
        }
      }
      break;
    }
    case State::passiveReceiveMasterAcknowledge: {
      if (byte == sym_ack) {
        if (passiveTelegram.getType() == Type::MM) {
          passiveCallback(passiveTelegram.getMaster().to_vector(),
                          passiveTelegram.getSlave().to_vector());
          counters.passiveMM++;
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
        counters.failure++;
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
        state = State::passiveReceiveSlaveAcknowledge;
      }
      break;
    }
    case State::passiveReceiveSlaveAcknowledge: {
      if (byte == sym_nak && !passiveSlaveRepeated) {
        passiveSlaveRepeated = true;
        state = State::passiveReceiveSlave;
        passiveSlave.clear();
        passiveSlaveDBx = 0;
      } else if (byte == sym_ack) {
        passiveCallback(passiveTelegram.getMaster().to_vector(),
                        passiveTelegram.getSlave().to_vector());
        counters.passiveMS++;
        state = State::passiveReceiveMaster;
        resetPassive();
      } else {
        counters.failure++;
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
        if (byte == sym_nak) counters.failure++;
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
          counters.requestLost1++;
          state = State::passiveReceiveMaster;
          active = false;
          activeTelegram.clear();
          activeMaster.clear();
        }
      } else {
        counters.requestWon1++;
        state = State::activeSendMaster;
        activeMasterSendIndex = 1;
        activeMasterReceiveIndex = 1;
      }
      break;
    }
    case State::requestBusPriorityRetry: {
      if (byte != sym_syn) {
        counters.requestError++;
        state = State::passiveReceiveMaster;
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
      } else {
        counters.requestRetry++;
        state = State::requestBusSecondTry;
        write = true;
      }
      break;
    }
    case State::requestBusSecondTry: {
      if (byte != address) {
        counters.requestLost2++;
        state = State::passiveReceiveMaster;
        active = false;
        activeTelegram.clear();
        activeMaster.clear();
      } else {
        counters.requestWon2++;
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
          counters.activeBC++;
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
          counters.activeMM++;
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
        counters.failure++;
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
      counters.activeMS++;
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
        counters.failure++;
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
    default:
      break;
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
    default:
      break;
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
