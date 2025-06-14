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
ebus::Handler::Handler(const uint8_t source) {
  setAddress(source);

  stateHandlers = {&Handler::passiveReceiveMaster,
                   &Handler::passiveReceiveMasterAcknowledge,
                   &Handler::passiveReceiveSlave,
                   &Handler::passiveReceiveSlaveAcknowledge,
                   &Handler::reactiveSendMasterPositiveAcknowledge,
                   &Handler::reactiveSendMasterNegativeAcknowledge,
                   &Handler::reactiveSendSlave,
                   &Handler::reactiveReceiveSlaveAcknowledge,
                   &Handler::requestBusFirstTry,
                   &Handler::requestBusPriorityRetry,
                   &Handler::requestBusSecondTry,
                   &Handler::activeSendMaster,
                   &Handler::activeReceiveMasterAcknowledge,
                   &Handler::activeReceiveSlave,
                   &Handler::activeSendSlavePositiveAcknowledge,
                   &Handler::activeSendSlaveNegativeAcknowledge,
                   &Handler::releaseBus};

  lastPoint = std::chrono::steady_clock::now();
}

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
  slaveAddress = slaveOf(address);
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
  calculateDuration(byte);

  size_t idx = static_cast<size_t>(state);
  if (idx < stateHandlers.size() && stateHandlers[idx]) {
    (this->*stateHandlers[idx])(byte);
  }

  calculateDurationFsmState(byte);
}

void ebus::Handler::resetCounters() {
#define X(name) counters.name = 0;
  EBUS_COUNTERS_LIST
#undef X
}

const ebus::Counters &ebus::Handler::getCounters() {
  counters.messagesTotal =
      counters.messagesPassiveMasterSlave +
      counters.messagesPassiveMasterMaster +
      counters.messagesReactiveMasterSlave +
      counters.messagesReactiveMasterMaster +
      counters.messagesReactiveBroadcast + counters.messagesActiveMasterSlave +
      counters.messagesActiveMasterMaster + counters.messagesActiveBroadcast;

  counters.requestsTotal =
      counters.requestsWon + counters.requestsLost + counters.requestsError;

  counters.resetsTotal = counters.resetsPassive00 + counters.resetsPassive0704 +
                         counters.resetsActive + counters.resetsPassive;

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

  return counters;
}

void ebus::Handler::resetTimings() {
  sync.clear();
  passiveFirst.clear();
  passiveData.clear();
  activeFirst.clear();
  activeData.clear();
}

const ebus::Timings &ebus::Handler::getTimings() {
#define X(name)                     \
  timings.name##Last = name.last;   \
  timings.name##Count = name.count; \
  timings.name##Mean = name.mean;   \
  timings.name##StdDev = name.stddev();
  EBUS_TIMINGS_LIST
#undef X
  return timings;
}

void ebus::Handler::resetStateTimingStats() {
  for (auto &stat : fsmTimingStats) stat.clear();
}

const ebus::StateTimingStatsResults ebus::Handler::getStateTimingStatsResults()
    const {
  StateTimingStatsResults results;
  for (size_t i = 0; i < fsmTimingStats.size(); ++i) {
    results.states[static_cast<FsmState>(i)] = {
        std::string(getFsmStateText(static_cast<FsmState>(i))),
        fsmTimingStats[i].last, fsmTimingStats[i].mean,
        fsmTimingStats[i].stddev(), fsmTimingStats[i].count};
  }
  return results;
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
            resetPassive();
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
          resetPassive();
        }
      }
    }
  } else {
    if (passiveMaster.size() == 1 && lockCounter == 0)
      lockCounter = 1;
    else if (passiveMaster.size() != 1 && lockCounter > 0)
      lockCounter--;

    checkPassiveBuffers();
    checkActiveBuffers();

    int available = 0;
    if (isDataAvailableCallback != nullptr)
      available = isDataAvailableCallback();

    if (lockCounter == 0 && available == 0 && active) {
      activeMaster = activeTelegram.getMaster();
      activeMaster.push_back(activeTelegram.getMasterCRC(), false);
      activeMaster.extend();
      onWriteCallback(address);
      state = FsmState::requestBusFirstTry;
      lastState = FsmState::requestBusFirstTry;
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
    if (passiveMaster.size() == 6 && passiveMaster[2] == 0x07 &&
        passiveMaster[3] == 0x04)
      counters.resetsPassive0704++;

    resetPassive();
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
    resetPassive();
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
    resetPassive();
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
    if (byte == sym_nak) counters.errorsReactiveSlaveACK++;
    resetPassive();
    state = FsmState::passiveReceiveMaster;
  }
}

void ebus::Handler::requestBusFirstTry(const uint8_t &byte) {
  if (byte != sym_syn) {
    if (byte != address) {
      if (!isMaster(byte) && (byte & 0x0f) == (address & 0x0f)) {
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
    resetActive();
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
    resetActive();
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
void ebus::Handler::checkPassiveBuffers() {
  if (passiveMaster.size() > 0 || passiveSlave.size() > 0) {
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
    else
      counters.resetsPassive++;

    resetPassive();
  }
}

/**
 * Handles errors that occur during active communication.
 *
 * This method is invoked when an error is detected in the active
 * communication process. It checks for inconsistencies or issues in the
 * active master and slave data structures, logs the error details using the
 * onErrorCallback if it is set, and increments the active reset counter.
 * Finally, it resets the active communication state to ensure the system can
 * recover and continue operating.
 */
void ebus::Handler::checkActiveBuffers() {
  if (activeMaster.size() > 0 || activeSlave.size() > 0) {
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

void ebus::Handler::calculateDuration(const uint8_t &byte) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  int64_t duration =
      std::chrono::duration_cast<std::chrono::microseconds>(now - lastPoint)
          .count();

  if (byte != sym_syn) {
    if (active) {
      if (measureSync)
        activeFirst.add(duration);
      else
        activeData.add(duration);
    } else {
      if (measureSync)
        passiveFirst.add(duration);
      else
        passiveData.add(duration);
    }
    measureSync = false;
  } else {
    if (measureSync) sync.add(duration);
    measureSync = true;
  }

  lastPoint = now;
}

void ebus::Handler::calculateDurationFsmState(const uint8_t &byte) {
  if (byte != sym_syn || lastState == FsmState::requestBusPriorityRetry) {
    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastPoint)
            .count();

    fsmTimingStats[static_cast<size_t>(lastState)].add(duration);
    lastState = state;
  }
}
