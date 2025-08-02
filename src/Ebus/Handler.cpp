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

#include <utility>
#include <vector>

#include "Common.hpp"

// Constructor initializes the Handler with the given source address.
// Calls setAddress to validate and set the address as either a master or
// default value.
ebus::Handler::Handler(Bus *bus, Request *request, const uint8_t source)
    : bus(bus), request(request) {
  setAddress(source);

  stateHandlers = {&Handler::passiveReceiveMaster,
                   &Handler::passiveReceiveMasterAcknowledge,
                   &Handler::passiveReceiveSlave,
                   &Handler::passiveReceiveSlaveAcknowledge,
                   &Handler::reactiveSendMasterPositiveAcknowledge,
                   &Handler::reactiveSendMasterNegativeAcknowledge,
                   &Handler::reactiveSendSlave,
                   &Handler::reactiveReceiveSlaveAcknowledge,
                   &Handler::requestBus,
                   &Handler::activeSendMaster,
                   &Handler::activeReceiveMasterAcknowledge,
                   &Handler::activeReceiveSlave,
                   &Handler::activeSendSlavePositiveAcknowledge,
                   &Handler::activeSendSlaveNegativeAcknowledge,
                   &Handler::releaseBus};

  lastPoint = std::chrono::steady_clock::now();
}

void ebus::Handler::setReactiveMasterSlaveCallback(
    ebus::ReactiveMasterSlaveCallback callback) {
  reactiveMasterSlaveCallback = std::move(callback);
}

void ebus::Handler::setTelegramCallback(ebus::TelegramCallback callback) {
  telegramCallback = std::move(callback);
}

void ebus::Handler::setErrorCallback(ebus::ErrorCallback callback) {
  errorCallback = std::move(callback);
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

ebus::HandlerState ebus::Handler::getState() const { return state; }

bool ebus::Handler::isActive() const { return active; }

bool ebus::Handler::busRequest() const { return requestFlag; }

void ebus::Handler::busRequested() {
  if (requestFlag) {
    requestFlag = false;
    if (state != HandlerState::requestBus) {
      state = HandlerState::requestBus;
      lastState = HandlerState::requestBus;
    }
  }
}

void ebus::Handler::busIsrStartBit() {
  if (requestFlag) {
    requestFlag = false;
    // counter.requestsStartBit++;
    callActiveReset();
  }
}

void ebus::Handler::reset() {
  state = HandlerState::passiveReceiveMaster;
  callActiveReset();
  callPassiveReset();
}

bool ebus::Handler::enque(const std::vector<uint8_t> &message) {
  if (message.empty()) return false;

  active = false;

  activeTelegram.createMaster(address, message);
  if (activeTelegram.getMasterState() == SequenceState::seq_ok) {
    active = true;
  } else {
    counter.errorActiveMaster++;
    callOnError("errorActiveMaster", activeMaster.to_vector(),
                activeSlave.to_vector());
  }

  return active;
}

void ebus::Handler::run(const uint8_t &byte) {
  calculateDuration(byte);

  size_t idx = static_cast<size_t>(state);
  if (idx < stateHandlers.size() && stateHandlers[idx])
    (this->*stateHandlers[idx])(byte);

  calculateDurationFsmState(byte);
}

void ebus::Handler::resetCounter() {
#define X(name) counter.name = 0;
  EBUS_HANDLER_COUNTER_LIST
#undef X

  request->resetCounter();
}

const ebus::Handler::Counter &ebus::Handler::getCounter() {
  counter.messagesTotal =
      counter.messagesPassiveMasterSlave + counter.messagesPassiveMasterMaster +
      counter.messagesPassiveBroadcast + counter.messagesActiveMasterSlave +
      counter.messagesActiveMasterMaster + counter.messagesActiveBroadcast +
      counter.messagesReactiveMasterSlave +
      counter.messagesReactiveMasterMaster;

  counter.resetTotal = counter.resetPassive00 + counter.resetPassive0704 +
                       counter.resetActive + counter.resetPassive;

  counter.errorPassive =
      counter.errorPassiveMaster + counter.errorPassiveMasterACK +
      counter.errorPassiveSlave + counter.errorPassiveSlaveACK;

  counter.errorReactive =
      counter.errorReactiveMaster + counter.errorReactiveMasterACK +
      counter.errorReactiveSlave + counter.errorReactiveSlaveACK;

  counter.errorActive = counter.errorActiveMaster +
                        counter.errorActiveMasterACK +
                        counter.errorActiveSlave + counter.errorActiveSlaveACK;

  counter.errorTotal =
      counter.errorPassive + counter.errorReactive + counter.errorActive;

  return counter;
}

void ebus::Handler::resetTiming() {
  sync.clear();
  write.clear();
  passiveFirst.clear();
  passiveData.clear();
  activeFirst.clear();
  activeData.clear();
  callbackReactive.clear();
  callbackTelegram.clear();
  callbackError.clear();

  resetStateTiming();
}

const ebus::Handler::Timing &ebus::Handler::getTiming() {
#define X(name)                    \
  timing.name##Last = name.last;   \
  timing.name##Count = name.count; \
  timing.name##Mean = name.mean;   \
  timing.name##StdDev = name.stddev();
  EBUS_HANDLER_TIMING_LIST
#undef X
  return timing;
}

void ebus::Handler::resetStateTiming() {
  for (ebus::TimingStats &stats : handlerTiming) stats.clear();
}

const ebus::Handler::StateTiming ebus::Handler::getStateTiming() const {
  StateTiming stateTiming;
  for (size_t i = 0; i < handlerTiming.size(); ++i) {
    stateTiming.timing[static_cast<HandlerState>(i)] = {
        std::string(getHandlerStateText(static_cast<HandlerState>(i))),
        handlerTiming[i].last, handlerTiming[i].mean, handlerTiming[i].stddev(),
        handlerTiming[i].count};
  }
  return stateTiming;
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
        if (passiveTelegram.getType() == TelegramType::broadcast) {
          callOnTelegram(MessageType::passive, TelegramType::broadcast,
                         passiveTelegram.getMaster().to_vector(),
                         passiveTelegram.getSlave().to_vector());
          counter.messagesPassiveBroadcast++;
          callPassiveReset();
        } else if (passiveMaster[1] == address) {
          callWrite(sym_ack);
          state = HandlerState::reactiveSendMasterPositiveAcknowledge;
        } else if (passiveMaster[1] == slaveAddress) {
          std::vector<uint8_t> response;
          callReactiveMasterSlave(passiveTelegram.getMaster().to_vector(),
                                  &response);
          passiveTelegram.createSlave(response);
          if (passiveTelegram.getSlaveState() == SequenceState::seq_ok) {
            passiveSlave = passiveTelegram.getSlave();
            passiveSlave.push_back(passiveTelegram.getSlaveCRC(), false);
            passiveSlave.extend();
            callWrite(sym_ack);
            state = HandlerState::reactiveSendMasterPositiveAcknowledge;
          } else {
            counter.errorReactiveSlave++;
            callOnError("errorReactiveSlave", passiveMaster.to_vector(),
                        passiveSlave.to_vector());
            callPassiveReset();
            callWrite(sym_syn);
            state = HandlerState::releaseBus;
          }
        } else {
          state = HandlerState::passiveReceiveMasterAcknowledge;
        }
      } else {
        if (passiveMaster[1] == address || passiveMaster[1] == slaveAddress) {
          counter.errorReactiveMaster++;
          callOnError("errorReactiveMaster", passiveMaster.to_vector(),
                      passiveSlave.to_vector());
          passiveTelegram.clear();
          passiveMaster.clear();
          passiveMasterDBx = 0;
          callWrite(sym_nak);
          state = HandlerState::reactiveSendMasterNegativeAcknowledge;
        } else if (passiveTelegram.getType() == TelegramType::master_master ||
                   passiveTelegram.getType() == TelegramType::master_slave) {
          state = HandlerState::passiveReceiveMasterAcknowledge;
        } else {
          counter.errorPassiveMaster++;
          callOnError("errorPassiveMaster", passiveMaster.to_vector(),
                      passiveSlave.to_vector());
          callPassiveReset();
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

    // Initiate request bus
    requestFlag = active && lockCounter == 0;
  }
}

void ebus::Handler::passiveReceiveMasterAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    if (passiveTelegram.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::passive, TelegramType::master_master,
                     passiveTelegram.getMaster().to_vector(),
                     passiveTelegram.getSlave().to_vector());
      counter.messagesPassiveMasterMaster++;
      callPassiveReset();
      state = HandlerState::passiveReceiveMaster;
    } else {
      state = HandlerState::passiveReceiveSlave;
    }
  } else if (byte != sym_syn && !passiveMasterRepeated) {
    passiveMasterRepeated = true;
    passiveTelegram.clear();
    passiveMaster.clear();
    passiveMasterDBx = 0;
    state = HandlerState::passiveReceiveMaster;
  } else {
    counter.errorPassiveMasterACK++;
    if (passiveMaster.size() == 6 && passiveMaster[2] == 0x07 &&
        passiveMaster[3] == 0x04)
      counter.resetPassive0704++;

    callOnError("errorPassiveMasterACK", passiveMaster.to_vector(),
                passiveSlave.to_vector());
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
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
    if (passiveTelegram.getSlaveState() != SequenceState::seq_ok) {
      counter.errorPassiveSlave++;
      callOnError("errorPassiveSlave", passiveMaster.to_vector(),
                  passiveSlave.to_vector());
    }
    state = HandlerState::passiveReceiveSlaveAcknowledge;
  }
}

void ebus::Handler::passiveReceiveSlaveAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::passive, TelegramType::master_slave,
                   passiveTelegram.getMaster().to_vector(),
                   passiveTelegram.getSlave().to_vector());
    counter.messagesPassiveMasterSlave++;
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated) {
    passiveSlaveRepeated = true;
    passiveSlave.clear();
    passiveSlaveDBx = 0;
    state = HandlerState::passiveReceiveSlave;
  } else {
    counter.errorPassiveSlaveACK++;
    callOnError("errorPassiveSlaveACK", passiveMaster.to_vector(),
                passiveSlave.to_vector());
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(const uint8_t &byte) {
  if (passiveTelegram.getType() == TelegramType::master_master) {
    callOnTelegram(MessageType::reactive, TelegramType::master_master,
                   passiveTelegram.getMaster().to_vector(),
                   passiveTelegram.getSlave().to_vector());
    counter.messagesReactiveMasterMaster++;
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
  } else {
    callWrite(passiveSlave[passiveSlaveIndex]);
    state = HandlerState::reactiveSendSlave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(const uint8_t &byte) {
  state = HandlerState::passiveReceiveMaster;
  if (!passiveMasterRepeated) {
    passiveMasterRepeated = true;
  } else {
    counter.errorReactiveMasterACK++;
    callOnError("errorReactiveMasterACK", passiveMaster.to_vector(),
                passiveSlave.to_vector());
    callPassiveReset();
  }
}

void ebus::Handler::reactiveSendSlave(const uint8_t &byte) {
  passiveSlaveIndex++;
  if (passiveSlaveIndex >= passiveSlave.size())
    state = HandlerState::reactiveReceiveSlaveAcknowledge;
  else
    callWrite(passiveSlave[passiveSlaveIndex]);
}

void ebus::Handler::reactiveReceiveSlaveAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::reactive, TelegramType::master_slave,
                   passiveTelegram.getMaster().to_vector(),
                   passiveTelegram.getSlave().to_vector());
    counter.messagesReactiveMasterSlave++;
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated) {
    passiveSlaveRepeated = true;
    passiveSlaveIndex = 0;
    callWrite(passiveSlave[passiveSlaveIndex]);
    state = HandlerState::reactiveSendSlave;
  } else {
    counter.errorReactiveSlaveACK++;
    callOnError("errorReactiveSlaveACK", passiveMaster.to_vector(),
                passiveSlave.to_vector());
    callPassiveReset();
    state = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::requestBus(const uint8_t &byte) {
  auto won = [&]() {
    activeMaster = activeTelegram.getMaster();
    activeMaster.push_back(activeTelegram.getMasterCRC(), false);
    activeMaster.extend();
    activeMasterIndex = 1;
    callWrite(activeMaster[activeMasterIndex]);
    state = HandlerState::activeSendMaster;
  };

  auto lost = [&]() {
    passiveMaster.push_back(byte);
    active = false;
    activeTelegram.clear();
    activeMaster.clear();
    state = HandlerState::passiveReceiveMaster;
  };

  auto error = [&]() {
    requestFlag = false;
    active = false;
    activeTelegram.clear();
    activeMaster.clear();
    state = HandlerState::passiveReceiveMaster;
  };

  switch (request->run(address, byte)) {
    case RequestResult::firstSyn:
      break;
    case RequestResult::firstWon:
      won();
      break;
    case RequestResult::firstRetry:
      break;
    case RequestResult::firstLost:
      lost();
      break;
    case RequestResult::firstError:
      lost();
      break;
    case RequestResult::retrySyn:
      break;
    case RequestResult::retryError:
      error();
      break;
    case RequestResult::secondWon:
      won();
      break;
    case RequestResult::secondLost:
      lost();
      break;
    case RequestResult::secondError:
      lost();
      break;

    default:
      break;
  }
}

void ebus::Handler::activeSendMaster(const uint8_t &byte) {
  activeMasterIndex++;
  if (activeMasterIndex >= activeMaster.size()) {
    if (activeTelegram.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     activeTelegram.getMaster().to_vector(),
                     activeTelegram.getSlave().to_vector());
      counter.messagesActiveBroadcast++;
      callActiveReset();
      callWrite(sym_syn);
      state = HandlerState::releaseBus;
    } else {
      state = HandlerState::activeReceiveMasterAcknowledge;
    }
  } else {
    callWrite(activeMaster[activeMasterIndex]);
  }
}

void ebus::Handler::activeReceiveMasterAcknowledge(const uint8_t &byte) {
  if (byte == sym_ack) {
    if (activeTelegram.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     activeTelegram.getMaster().to_vector(),
                     activeTelegram.getSlave().to_vector());
      counter.messagesActiveMasterMaster++;
      callActiveReset();
      callWrite(sym_syn);
      state = HandlerState::releaseBus;
    } else {
      state = HandlerState::activeReceiveSlave;
    }
  } else if (byte == sym_nak && !activeMasterRepeated) {
    activeMasterRepeated = true;
    activeMasterIndex = 0;
    callWrite(activeMaster[activeMasterIndex]);
    state = HandlerState::activeSendMaster;
  } else {
    counter.errorActiveMasterACK++;
    callOnError("errorActiveMasterACK", activeMaster.to_vector(),
                activeSlave.to_vector());
    callActiveReset();
    callWrite(sym_syn);
    state = HandlerState::releaseBus;
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
      callWrite(sym_ack);
      state = HandlerState::activeSendSlavePositiveAcknowledge;
    } else {
      counter.errorActiveSlave++;
      callOnError("errorActiveSlave", activeMaster.to_vector(),
                  activeSlave.to_vector());
      activeSlave.clear();
      activeSlaveDBx = 0;
      callWrite(sym_nak);
      state = HandlerState::activeSendSlaveNegativeAcknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(const uint8_t &byte) {
  callOnTelegram(MessageType::active, TelegramType::master_slave,
                 activeTelegram.getMaster().to_vector(),
                 activeTelegram.getSlave().to_vector());
  counter.messagesActiveMasterSlave++;
  callActiveReset();
  callWrite(sym_syn);
  state = HandlerState::releaseBus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(const uint8_t &byte) {
  if (!activeSlaveRepeated) {
    activeSlaveRepeated = true;
    state = HandlerState::activeReceiveSlave;
  } else {
    counter.errorActiveSlaveACK++;
    callOnError("errorActiveSlaveACK", activeMaster.to_vector(),
                activeSlave.to_vector());
    callActiveReset();
    callWrite(sym_syn);
    state = HandlerState::releaseBus;
  }
}

void ebus::Handler::releaseBus(const uint8_t &byte) {
  state = HandlerState::passiveReceiveMaster;
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
    callOnError("checkPassiveBuffers", passiveMaster.to_vector(),
                passiveSlave.to_vector());

    if (passiveMaster.size() == 1 && passiveMaster[0] == 0x00)
      counter.resetPassive00++;
    else
      counter.resetPassive++;

    callPassiveReset();
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
    callOnError("checkActiveBuffers", activeMaster.to_vector(),
                activeSlave.to_vector());
    counter.resetActive++;
    callActiveReset();
  }
}

void ebus::Handler::callPassiveReset() {
  passiveTelegram.clear();

  passiveMaster.clear();
  passiveMasterDBx = 0;
  passiveMasterRepeated = false;

  passiveSlave.clear();
  passiveSlaveDBx = 0;
  passiveSlaveIndex = 0;
  passiveSlaveRepeated = false;
}

void ebus::Handler::callActiveReset() {
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
  if (byte != sym_syn) {
    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(now - lastPoint)
            .count();

    handlerTiming[static_cast<size_t>(lastState)].add(duration);
    lastState = state;
  }
}

void ebus::Handler::callWrite(const uint8_t &byte) {
  if (bus) {
    std::chrono::steady_clock::time_point t_start =
        std::chrono::steady_clock::now();

    bus->writeByte(byte);

    std::chrono::steady_clock::time_point t_end =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start)
            .count();

    write.add(duration);
  }
}

void ebus::Handler::callReactiveMasterSlave(const std::vector<uint8_t> &master,
                                            std::vector<uint8_t> *const slave) {
  if (reactiveMasterSlaveCallback) {
    std::chrono::steady_clock::time_point t_start =
        std::chrono::steady_clock::now();

    reactiveMasterSlaveCallback(master, slave);

    std::chrono::steady_clock::time_point t_end =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start)
            .count();

    callbackReactive.add(duration);
  }
}

void ebus::Handler::callOnTelegram(const MessageType &messageType,
                                   const TelegramType &telegramType,
                                   const std::vector<uint8_t> &master,
                                   const std::vector<uint8_t> &slave) {
  if (telegramCallback) {
    std::chrono::steady_clock::time_point t_start =
        std::chrono::steady_clock::now();

    telegramCallback(messageType, telegramType, master, slave);

    std::chrono::steady_clock::time_point t_end =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start)
            .count();

    callbackTelegram.add(duration);
  }
}

void ebus::Handler::callOnError(const std::string &error,
                                const std::vector<uint8_t> &master,
                                const std::vector<uint8_t> &slave) {
  if (errorCallback) {
    std::chrono::steady_clock::time_point t_start =
        std::chrono::steady_clock::now();

    errorCallback(error, master, slave);

    std::chrono::steady_clock::time_point t_end =
        std::chrono::steady_clock::now();
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start)
            .count();

    callbackError.add(duration);
  }
}
