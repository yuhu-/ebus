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

#include "Handler.hpp"

#include <sstream>
#include <utility>
#include <vector>

#include "Common.hpp"

// Constructor initializes the Handler with the given source address.
// Calls setAddress to validate and set the address as either a master or
// default value.
ebus::Handler::Handler(const uint8_t source) { setAddress(source); }

void ebus::Handler::onWrite(ebus::OnWriteCallback callback) {
  onWriteCallback = callback;
}

void ebus::Handler::isDataAvailable(ebus::IsDataAvailableCallback callback) {
  isDataAvailableCallback = callback;
}

void ebus::Handler::onTelegram(ebus::OnTelegramCallback callback) {
  onTelegramCallback = callback;
}

void ebus::Handler::onError(ebus::OnErrorCallback callback) {
  onErrorCallback = callback;
}

void ebus::Handler::setAddress(const uint8_t source) {
  address = ebus::isMaster(source) ? source : DEFAULT_ADDRESS;
  slaveAddress = slaveAddressOf(address);
}

uint8_t ebus::Handler::getAddress() const { return address; }

uint8_t ebus::Handler::getSlaveAddress() const { return slaveAddress; }

void ebus::Handler::setMaxLockCounter(const uint8_t counter) {
  if (counter > MAX_LOCK_COUNTER)
    maxLockCounter = DEFAULT_LOCK_COUNTER;
  else
    maxLockCounter = counter;

  lockCounter = maxLockCounter;
}

ebus::FsmState ebus::Handler::getState() const { return state; }

bool ebus::Handler::isActive() const { return active; }

void ebus::Handler::reset() {
  state = FsmState::passiveReceiveMaster;
  resetActive();
  resetPassive();
}

bool ebus::Handler::enque(const std::vector<uint8_t> &message) {
  if (onWriteCallback == nullptr || message.empty()) return false;

  active = false;

  activeTelegram.createMaster(address, message);
  if (activeTelegram.getMasterState() == SequenceState::seq_ok)
    active = true;
  else
    counters.errorsActiveMaster++;

  return active;
}

void ebus::Handler::run(const uint8_t &byte) {
  switch (state) {
    case FsmState::passiveReceiveMaster: {
      passiveReceiveMaster(byte);
      break;
    }
    case FsmState::passiveReceiveMasterAcknowledge: {
      passiveReceiveMasterAcknowledge(byte);
      break;
    }
    case FsmState::passiveReceiveSlave: {
      passiveReceiveSlave(byte);
      break;
    }
    case FsmState::passiveReceiveSlaveAcknowledge: {
      passiveReceiveSlaveAcknowledge(byte);
      break;
    }
    case FsmState::reactiveSendMasterPositiveAcknowledge: {
      reactiveSendMasterPositiveAcknowledge(byte);
      break;
    }
    case FsmState::reactiveSendMasterNegativeAcknowledge: {
      reactiveSendMasterNegativeAcknowledge(byte);
      break;
    }
    case FsmState::reactiveSendSlave: {
      reactiveSendSlave(byte);
      break;
    }
    case FsmState::reactiveReceiveSlaveAcknowledge: {
      reactiveReceiveSlaveAcknowledge(byte);
      break;
    }
    case FsmState::requestBusFirstTry: {
      requestBusFirstTry(byte);
      break;
    }
    case FsmState::requestBusPriorityRetry: {
      requestBusPriorityRetry(byte);
      break;
    }
    case FsmState::requestBusSecondTry: {
      requestBusSecondTry(byte);
      break;
    }
    case FsmState::activeSendMaster: {
      activeSendMaster(byte);
      break;
    }
    case FsmState::activeReceiveMasterAcknowledge: {
      activeReceiveMasterAcknowledge(byte);
      break;
    }
    case FsmState::activeReceiveSlave: {
      activeReceiveSlave(byte);
      break;
    }
    case FsmState::activeSendSlavePositiveAcknowledge: {
      activeSendSlavePositiveAcknowledge(byte);
      break;
    }
    case FsmState::activeSendSlaveNegativeAcknowledge: {
      activeSendSlaveNegativeAcknowledge(byte);
      break;
    }
    case FsmState::releaseBus: {
      releaseBus(byte);
      break;
    }
  }
}

void ebus::Handler::resetCounters() {
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

const ebus::Counters &ebus::Handler::getCounters() {
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

void ebus::Handler::passiveReceiveMaster(const uint8_t &byte) {
  if (byte != sym_syn) {
    passiveMaster.push_back(byte);

    if (passiveMaster.size() == 5) passiveMasterDBx = passiveMaster[4];

    // AA >> A9 + 01 || A9 >> A9 + 00
    if (byte == sym_ext) passiveMasterDBx++;

    // size() > ZZ QQ PB SB NN + DBx + CRC
    if (passiveMaster.size() >= 5 + passiveMasterDBx + 1) {
      passiveTelegram.createMaster(passiveMaster);
      if (passiveTelegram.getMasterState() == SequenceState::seq_ok) {
        std::vector<uint8_t> response;
        if (passiveTelegram.getType() == TelegramType::broadcast) {
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(MessageType::reactive, TelegramType::broadcast,
                               passiveTelegram.getMaster().to_vector(),
                               &response);
          }
          counters.messagesReactiveBroadcast++;
          resetPassive();
        } else if (passiveMaster[1] == address) {
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(
                MessageType::reactive, TelegramType::master_master,
                passiveTelegram.getMaster().to_vector(), &response);
          }
          counters.messagesReactiveMasterMaster++;
          onWriteCallback(sym_ack);
          state = FsmState::reactiveSendMasterPositiveAcknowledge;
        } else if (passiveMaster[1] == slaveAddress) {
          if (onTelegramCallback != nullptr) {
            onTelegramCallback(
                MessageType::reactive, TelegramType::master_slave,
                passiveTelegram.getMaster().to_vector(), &response);
          }
          counters.messagesReactiveMasterSlave++;
          passiveTelegram.createSlave(response);
          if (passiveTelegram.getSlaveState() == SequenceState::seq_ok) {
            passiveSlave = passiveTelegram.getSlave();
            passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
            passiveSlave.extend();
            onWriteCallback(sym_ack);
            state = FsmState::reactiveSendMasterPositiveAcknowledge;
          } else {
            counters.errorsReactiveSlave++;
            handlePassiveErrors();
            onWriteCallback(sym_syn);
            state = FsmState::releaseBus;
          }
        } else {
          state = FsmState::passiveReceiveMasterAcknowledge;
        }
      } else {
        if (passiveMaster[1] == address || passiveMaster[1] == slaveAddress) {
          counters.errorsReactiveMaster++;
          passiveTelegram.clear();
          passiveMaster.clear();
          passiveMasterDBx = 0;
          onWriteCallback(sym_nak);
          state = FsmState::reactiveSendMasterNegativeAcknowledge;
        } else if (passiveTelegram.getType() == TelegramType::master_master ||
                   passiveTelegram.getType() == TelegramType::master_slave) {
          state = FsmState::passiveReceiveMasterAcknowledge;
        } else {
          counters.errorsPassiveMaster++;
          handlePassiveErrors();
        }
      }
    }
  } else {
    if (passiveMaster.size() != 1 && lockCounter > 0) lockCounter--;

    handlePassiveErrors();
    handleActiveErrors();

    int available = 0;
    if (isDataAvailableCallback != nullptr)
      available = isDataAvailableCallback();

    if (lockCounter == 0 && available == 0 && active) {
      activeMaster = activeTelegram.getMaster();
      activeMaster.push_back(activeTelegram.getMasterCRC(), false);
      activeMaster.extend();
      onWriteCallback(address);
      state = FsmState::requestBusFirstTry;
    }
  }
}

void ebus::Handler::passiveReceiveMasterAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    if (passiveTelegram.getType() == TelegramType::master_master) {
      if (onTelegramCallback != nullptr) {
        onTelegramCallback(MessageType::passive, TelegramType::master_master,
                           passiveTelegram.getMaster().to_vector(),
                           &const_cast<std::vector<uint8_t> &>(
                               (passiveTelegram.getSlave().to_vector())));
      }
      counters.messagesPassiveMasterMaster++;
      resetPassive();
      state = FsmState::passiveReceiveMaster;
    } else {
      state = FsmState::passiveReceiveSlave;
    }
  } else if (byte != sym_syn && !passiveMasterRepeated) {
    passiveMasterRepeated = true;
    passiveTelegram.clear();
    passiveMaster.clear();
    passiveMasterDBx = 0;
    state = FsmState::passiveReceiveMaster;
  } else {
    counters.errorsPassiveMasterACK++;
    handlePassiveErrors();
    state = FsmState::passiveReceiveMaster;
  }
}

void ebus::Handler::passiveReceiveSlave(const uint8_t &byte) {
  passiveSlave.push_back(byte);

  if (passiveSlave.size() == 1) passiveSlaveDBx = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) passiveSlaveDBx++;

  // size() > NN + DBx + CRC
  if (passiveSlave.size() >= 1 + passiveSlaveDBx + 1) {
    passiveTelegram.createSlave(passiveSlave);
    if (passiveTelegram.getSlaveState() != SequenceState::seq_ok)
      counters.errorsPassiveSlave++;
    state = FsmState::passiveReceiveSlaveAcknowledge;
  }
}

void ebus::Handler::passiveReceiveSlaveAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    if (onTelegramCallback != nullptr) {
      onTelegramCallback(MessageType::passive, TelegramType::master_slave,
                         passiveTelegram.getMaster().to_vector(),
                         &const_cast<std::vector<uint8_t> &>(
                             (passiveTelegram.getSlave().to_vector())));
    }
    counters.messagesPassiveMasterSlave++;
    resetPassive();
    state = FsmState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated) {
    passiveSlaveRepeated = true;
    passiveSlave.clear();
    passiveSlaveDBx = 0;
    state = FsmState::passiveReceiveSlave;
  } else {
    counters.errorsPassiveSlaveACK++;
    handlePassiveErrors();
    state = FsmState::passiveReceiveMaster;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(const uint8_t &byte) {
  if (passiveTelegram.getType() == TelegramType::master_master) {
    resetPassive();
    state = FsmState::passiveReceiveMaster;
  } else {
    onWriteCallback(passiveSlave[passiveSlaveIndex]);
    state = FsmState::reactiveSendSlave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(const uint8_t &byte) {
  state = FsmState::passiveReceiveMaster;
  if (!passiveMasterRepeated) {
    passiveMasterRepeated = true;
  } else {
    counters.errorsReactiveMasterACK++;
    handlePassiveErrors();
  }
}

void ebus::Handler::reactiveSendSlave(const uint8_t &byte) {
  passiveSlaveIndex++;
  if (passiveSlaveIndex >= passiveSlave.size())
    state = FsmState::reactiveReceiveSlaveAcknowledge;
  else
    onWriteCallback(passiveSlave[passiveSlaveIndex]);
}

void ebus::Handler::reactiveReceiveSlaveAcknowledge(const uint8_t &byte) {
  if (byte == sym_nak && !passiveSlaveRepeated) {
    passiveSlaveRepeated = true;
    passiveSlaveIndex = 0;
    onWriteCallback(passiveSlave[passiveSlaveIndex]);
    state = FsmState::reactiveSendSlave;
  } else {
    if (byte == sym_nak) {
      counters.errorsReactiveSlaveACK++;
      handlePassiveErrors();
    } else {
      resetPassive();
    }
    state = FsmState::passiveReceiveMaster;
  }
}

void ebus::Handler::requestBusFirstTry(const uint8_t &byte) {
  if (byte != address) {
    if ((byte & 0x0f) == (address & 0x0f)) {
      state = FsmState::requestBusPriorityRetry;
    } else {
      counters.requestsLost++;
      passiveMaster.push_back(byte);
      active = false;
      activeTelegram.clear();
      activeMaster.clear();
      state = FsmState::passiveReceiveMaster;
    }
  } else {
    counters.requestsWon++;
    activeMasterIndex = 1;
    onWriteCallback(activeMaster[activeMasterIndex]);
    state = FsmState::activeSendMaster;
  }
}

void ebus::Handler::requestBusPriorityRetry(const uint8_t &byte) {
  if (byte != sym_syn) {
    counters.requestsError++;
    active = false;
    activeTelegram.clear();
    activeMaster.clear();
    state = FsmState::passiveReceiveMaster;
  } else {
    counters.requestsRetry++;
    onWriteCallback(address);
    state = FsmState::requestBusSecondTry;
  }
}

void ebus::Handler::requestBusSecondTry(const uint8_t &byte) {
  if (byte != address) {
    counters.requestsLost++;
    passiveMaster.push_back(byte);
    active = false;
    activeTelegram.clear();
    activeMaster.clear();
    state = FsmState::passiveReceiveMaster;
  } else {
    counters.requestsWon++;
    activeMasterIndex = 1;
    onWriteCallback(activeMaster[activeMasterIndex]);
    state = FsmState::activeSendMaster;
  }
}

void ebus::Handler::activeSendMaster(const uint8_t &byte) {
  activeMasterIndex++;
  if (activeMasterIndex >= activeMaster.size()) {
    if (activeTelegram.getType() == TelegramType::broadcast) {
      if (onTelegramCallback != nullptr) {
        onTelegramCallback(MessageType::active, TelegramType::broadcast,
                           activeTelegram.getMaster().to_vector(),
                           &const_cast<std::vector<uint8_t> &>(
                               (activeTelegram.getSlave().to_vector())));
      }
      counters.messagesActiveBroadcast++;
      resetActive();
      onWriteCallback(sym_syn);
      state = FsmState::releaseBus;
    } else {
      state = FsmState::activeReceiveMasterAcknowledge;
    }
  } else {
    onWriteCallback(activeMaster[activeMasterIndex]);
  }
}

void ebus::Handler::activeReceiveMasterAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    if (activeTelegram.getType() == TelegramType::master_master) {
      if (onTelegramCallback != nullptr) {
        onTelegramCallback(MessageType::active, TelegramType::master_master,
                           activeTelegram.getMaster().to_vector(),
                           &const_cast<std::vector<uint8_t> &>(
                               (activeTelegram.getSlave().to_vector())));
      }
      counters.messagesActiveMasterMaster++;
      resetActive();
      onWriteCallback(sym_syn);
      state = FsmState::releaseBus;
    } else {
      state = FsmState::activeReceiveSlave;
    }
  } else if (!activeMasterRepeated) {
    activeMasterRepeated = true;
    activeMasterIndex = 0;
    onWriteCallback(activeMaster[activeMasterIndex]);
    state = FsmState::activeSendMaster;
  } else {
    counters.errorsActiveMasterACK++;
    handleActiveErrors();
    onWriteCallback(sym_syn);
    state = FsmState::releaseBus;
  }
}

void ebus::Handler::activeReceiveSlave(const uint8_t &byte) {
  activeSlave.push_back(byte);

  if (activeSlave.size() == 1) activeSlaveDBx = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) activeSlaveDBx++;

  // size() > NN + DBx + CRC
  if (activeSlave.size() >= 1 + activeSlaveDBx + 1) {
    activeTelegram.createSlave(activeSlave);
    if (activeTelegram.getSlaveState() == SequenceState::seq_ok) {
      onWriteCallback(sym_ack);
      state = FsmState::activeSendSlavePositiveAcknowledge;
    } else {
      counters.errorsActiveSlave++;
      activeSlave.clear();
      activeSlaveDBx = 0;
      onWriteCallback(sym_nak);
      state = FsmState::activeSendSlaveNegativeAcknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(const uint8_t &byte) {
  if (onTelegramCallback != nullptr) {
    onTelegramCallback(MessageType::active, TelegramType::master_slave,
                       activeTelegram.getMaster().to_vector(),
                       &const_cast<std::vector<uint8_t> &>(
                           (activeTelegram.getSlave().to_vector())));
  }
  counters.messagesActiveMasterSlave++;
  resetActive();
  onWriteCallback(sym_syn);
  state = FsmState::releaseBus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(const uint8_t &byte) {
  if (!activeSlaveRepeated) {
    activeSlaveRepeated = true;
    state = FsmState::activeReceiveSlave;
  } else {
    counters.errorsActiveSlaveACK++;
    handleActiveErrors();
    onWriteCallback(sym_syn);
    state = FsmState::releaseBus;
  }
}

void ebus::Handler::releaseBus(const uint8_t &byte) {
  state = FsmState::passiveReceiveMaster;
}

/**
 * Handles errors that occur during passive operations.
 *
 * This method is triggered when inconsistencies or issues are detected
 * in the passive master or slave communication sequences.
 * It logs the error details using the onErrorCallback if available,
 * updates the appropriate counters, and resets the passive state.
 */
void ebus::Handler::handlePassiveErrors() {
  if (passiveMaster.size() > 0 || passiveMasterDBx > 0 ||
      passiveMasterRepeated || passiveSlave.size() > 0 || passiveSlaveDBx > 0 ||
      passiveSlaveIndex > 0 || passiveSlaveRepeated) {
    if (onErrorCallback != nullptr) {
      std::ostringstream ostr;
      ostr << "passive";
      ostr << " | master: '" << passiveMaster.to_string();
      ostr << "' DBx: " << passiveMasterDBx;
      ostr << " repeated: " << (passiveMasterRepeated ? "true" : "false");
      ostr << " | slave: '" << passiveSlave.to_string();
      ostr << "' DBx: " << passiveSlaveDBx;
      ostr << " index: " << passiveSlaveIndex;
      ostr << " repeated: " << (passiveSlaveRepeated ? "true" : "false");
      onErrorCallback(ostr.str());
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

/**
 * Handles errors that occur during active communication.
 *
 * This method is invoked when an error is detected in the active communication
 * process. It checks for inconsistencies or issues in the active master and
 * slave data structures, logs the error details using the onErrorCallback
 * if it is set, and increments the active reset counter. Finally, it resets
 * the active communication state to ensure the system can recover and continue
 * operating.
 */
void ebus::Handler::handleActiveErrors() {
  if (activeMaster.size() > 0 || activeMasterIndex > 0 ||
      activeMasterRepeated || activeSlave.size() > 0 || activeSlaveDBx > 0 ||
      activeSlaveRepeated) {
    if (onErrorCallback != nullptr) {
      std::ostringstream ostr;
      ostr << "active";
      ostr << " | master: '" << activeMaster.to_string();
      ostr << "' index: " << activeMasterIndex;
      ostr << " repeated: " << (activeMasterRepeated ? "true" : "false");
      ostr << " | slave: '" << activeSlave.to_string();
      ostr << "' DBx: " << activeSlaveDBx;
      ostr << " repeated: " << (activeSlaveRepeated ? "true" : "false");
      onErrorCallback(ostr.str());
    }

    counters.resetsActive++;
    resetActive();
  }
}

void ebus::Handler::resetPassive() {
  passiveTelegram.clear();

  passiveMaster.clear();
  passiveMasterDBx = 0;
  passiveMasterRepeated = false;

  passiveSlave.clear();
  passiveSlaveDBx = 0;
  passiveSlaveIndex = 0;
  passiveSlaveRepeated = false;
}

void ebus::Handler::resetActive() {
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
