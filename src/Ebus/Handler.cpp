/*
 * Copyright (C) 2023-2026 Roland Jax
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

ebus::Handler::Handler(const uint8_t& address, Bus* bus, Request* request)
    : bus_(bus), request_(request) {
  setSourceAddress(address);

  request_->setHandlerBusRequestedCallback([this]() {
    if (activeMessage_ && state_ != HandlerState::requestBus)
      state_ = HandlerState::requestBus;
  });

  request_->setStartBitCallback([this]() {
    if (activeMessage_) callActiveReset();
  });

  stateHandlers_ = {&Handler::passiveReceiveMaster,
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

  lastPoint_ = std::chrono::steady_clock::now();
}

void ebus::Handler::setSourceAddress(const uint8_t& address) {
  sourceAddress_ = ebus::isMaster(address) ? address : DEFAULT_ADDRESS;
  targetAddress_ = slaveOf(sourceAddress_);
}

uint8_t ebus::Handler::getSourceAddress() const { return sourceAddress_; }

uint8_t ebus::Handler::getTargetAddress() const { return targetAddress_; }

void ebus::Handler::setBusRequestWonCallback(BusRequestWonCallback callback) {
  busRequestWonCallback_ = std::move(callback);
}

void ebus::Handler::setBusRequestLostCallback(BusRequestLostCallback callback) {
  busRequestLostCallback_ = std::move(callback);
}

void ebus::Handler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  reactiveMasterSlaveCallback_ = std::move(callback);
}

void ebus::Handler::setTelegramCallback(TelegramCallback callback) {
  telegramCallback_ = std::move(callback);
}

void ebus::Handler::setErrorCallback(ErrorCallback callback) {
  errorCallback_ = std::move(callback);
}

ebus::HandlerState ebus::Handler::getState() const { return state_; }

bool ebus::Handler::sendActiveMessage(const std::vector<uint8_t>& message) {
  if (activeMessage_) return false;
  if (message.empty()) return false;

  activeTelegram_.createMaster(sourceAddress_, message);
  if (activeTelegram_.getMasterState() == SequenceState::seq_ok) {
    activeMessage_ = true;
  } else {
    counter_.errorActiveMaster++;
    callOnError("errorActiveMaster", activeMaster.to_vector(),
                activeSlave_.to_vector());
  }

  return activeMessage_;
}

bool ebus::Handler::isActiveMessagePending() const { return activeMessage_; }

void ebus::Handler::reset() {
  state_ = HandlerState::passiveReceiveMaster;
  callActiveReset();
  callPassiveReset();
}

void ebus::Handler::run(const uint8_t& byte) {
  // record timing
  if (byte != sym_syn) {
    if (activeMessage_) {
      if (measureSync_)
        activeFirst_.addDurationWithTime(lastPoint_);
      else
        activeData_.addDurationWithTime(lastPoint_);
    } else {
      if (measureSync_)
        passiveFirst_.addDurationWithTime(lastPoint_);
      else
        passiveData_.addDurationWithTime(lastPoint_);
    }
    measureSync_ = false;
  } else {
    if (measureSync_) sync_.addDurationWithTime(lastPoint_);
    measureSync_ = true;
  }

  lastPoint_ = std::chrono::steady_clock::now();

  size_t idx = static_cast<size_t>(state_);
  if (idx < stateHandlers_.size() && stateHandlers_[idx]) {
    (this->*stateHandlers_[idx])(byte);  // handle byte
    if (byte != sym_syn || idx == static_cast<size_t>(HandlerState::releaseBus))
      handlerTiming_[static_cast<size_t>(idx)].addDurationWithTime(lastPoint_);
  }
}

void ebus::Handler::resetCounter() {
#define X(name) counter_.name = 0;
  EBUS_HANDLER_COUNTER_LIST
#undef X
}

const ebus::Handler::Counter& ebus::Handler::getCounter() {
  counter_.messagesTotal =
      counter_.messagesPassiveMasterSlave +
      counter_.messagesPassiveMasterMaster + counter_.messagesPassiveBroadcast +
      counter_.messagesActiveMasterSlave + counter_.messagesActiveMasterMaster +
      counter_.messagesActiveBroadcast + counter_.messagesReactiveMasterSlave +
      counter_.messagesReactiveMasterMaster;

  counter_.resetTotal = counter_.resetPassive00 + counter_.resetPassive0704 +
                        counter_.resetPassive + counter_.resetActive00 +
                        counter_.resetActive0704 + counter_.resetActive;

  counter_.errorPassive =
      counter_.errorPassiveMaster + counter_.errorPassiveMasterACK +
      counter_.errorPassiveSlave + counter_.errorPassiveSlaveACK;

  counter_.errorReactive =
      counter_.errorReactiveMaster + counter_.errorReactiveMasterACK +
      counter_.errorReactiveSlave + counter_.errorReactiveSlaveACK;

  counter_.errorActive =
      counter_.errorActiveMaster + counter_.errorActiveMasterACK +
      counter_.errorActiveSlave + counter_.errorActiveSlaveACK;

  counter_.errorTotal =
      counter_.errorPassive + counter_.errorReactive + counter_.errorActive;

  return counter_;
}

void ebus::Handler::resetTiming() {
  sync_.clear();
  write_.clear();
  passiveFirst_.clear();
  passiveData_.clear();
  activeFirst_.clear();
  activeData_.clear();
  callbackReactive_.clear();
  callbackTelegram_.clear();
  callbackError_.clear();

  resetStateTiming();
}

const ebus::Handler::Timing& ebus::Handler::getTiming() {
#define X(name)                           \
  {                                       \
    auto values = name.getValues();       \
    timing_.name##Last = values.last;     \
    timing_.name##Count = values.count;   \
    timing_.name##Mean = values.mean;     \
    timing_.name##StdDev = values.stddev; \
  }
  EBUS_HANDLER_TIMING_LIST
#undef X
  return timing_;
}

void ebus::Handler::resetStateTiming() {
  for (ebus::TimingStats& stats : handlerTiming_) stats.clear();
}

const ebus::Handler::StateTiming ebus::Handler::getStateTiming() const {
  StateTiming stateTiming;
  for (size_t i = 0; i < handlerTiming_.size(); ++i) {
    auto values = handlerTiming_[i].getValues();
    stateTiming.timing[static_cast<HandlerState>(i)] = {
        std::string(getHandlerStateText(static_cast<HandlerState>(i))),
        values.last, values.count, values.mean, values.stddev};
  }
  return stateTiming;
}

void ebus::Handler::passiveReceiveMaster(const uint8_t& byte) {
  if (byte != sym_syn) {
    passiveMaster_.push_back(byte);

    if (passiveMaster_.size() == 5) passiveMasterDBx_ = passiveMaster_[4];

    // AA >> A9 + 01 || A9 >> A9 + 00
    if (byte == sym_ext) passiveMasterDBx_++;

    // size() > ZZ QQ PB SB NN + DBx + CRC
    if (passiveMaster_.size() >= 5 + passiveMasterDBx_ + 1) {
      passiveTelegram_.createMaster(passiveMaster_);
      if (passiveTelegram_.getMasterState() == SequenceState::seq_ok) {
        if (passiveTelegram_.getType() == TelegramType::broadcast) {
          callOnTelegram(MessageType::passive, TelegramType::broadcast,
                         passiveTelegram_.getMaster().to_vector(),
                         passiveTelegram_.getSlave().to_vector());
          counter_.messagesPassiveBroadcast++;
          callPassiveReset();
        } else if (passiveMaster_[1] == sourceAddress_) {
          callWrite(sym_ack);
          state_ = HandlerState::reactiveSendMasterPositiveAcknowledge;
        } else if (passiveMaster_[1] == targetAddress_) {
          std::vector<uint8_t> response;
          callOnReactiveMasterSlave(passiveTelegram_.getMaster().to_vector(),
                                    &response);
          passiveTelegram_.createSlave(response);
          if (passiveTelegram_.getSlaveState() == SequenceState::seq_ok) {
            passiveSlave_ = passiveTelegram_.getSlave();
            passiveSlave_.push_back(passiveTelegram_.getSlaveCRC(), false);
            passiveSlave_.extend();
            callWrite(sym_ack);
            state_ = HandlerState::reactiveSendMasterPositiveAcknowledge;
          } else {
            counter_.errorReactiveSlave++;
            callOnError("errorReactiveSlave", passiveMaster_.to_vector(),
                        passiveSlave_.to_vector());
            callPassiveReset();
            callWrite(sym_syn);
            state_ = HandlerState::releaseBus;
          }
        } else {
          state_ = HandlerState::passiveReceiveMasterAcknowledge;
        }
      } else {
        if (passiveMaster_[1] == sourceAddress_ ||
            passiveMaster_[1] == targetAddress_) {
          counter_.errorReactiveMaster++;
          callOnError("errorReactiveMaster", passiveMaster_.to_vector(),
                      passiveSlave_.to_vector());
          passiveTelegram_.clear();
          passiveMaster_.clear();
          passiveMasterDBx_ = 0;
          callWrite(sym_nak);
          state_ = HandlerState::reactiveSendMasterNegativeAcknowledge;
        } else if (passiveTelegram_.getType() == TelegramType::master_master ||
                   passiveTelegram_.getType() == TelegramType::master_slave) {
          state_ = HandlerState::passiveReceiveMasterAcknowledge;
        } else {
          counter_.errorPassiveMaster++;
          callOnError("errorPassiveMaster", passiveMaster_.to_vector(),
                      passiveSlave_.to_vector());
          callPassiveReset();
        }
      }
    }
  } else {
    checkPassiveBuffers();
    checkActiveBuffers();

    // Initiate request bus
    if (activeMessage_) request_->requestBus(sourceAddress_);
  }
}

void ebus::Handler::passiveReceiveMasterAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    if (passiveTelegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::passive, TelegramType::master_master,
                     passiveTelegram_.getMaster().to_vector(),
                     passiveTelegram_.getSlave().to_vector());
      counter_.messagesPassiveMasterMaster++;
      callPassiveReset();
      state_ = HandlerState::passiveReceiveMaster;
    } else {
      state_ = HandlerState::passiveReceiveSlave;
    }
  } else if (byte != sym_syn && !passiveMasterRepeated_) {
    passiveMasterRepeated_ = true;
    passiveTelegram_.clear();
    passiveMaster_.clear();
    passiveMasterDBx_ = 0;
    state_ = HandlerState::passiveReceiveMaster;
  } else {
    counter_.errorPassiveMasterACK++;
    if (passiveMaster_.size() == 6 && passiveMaster_[2] == 0x07 &&
        passiveMaster_[3] == 0x04)
      counter_.resetPassive0704++;

    callOnError("errorPassiveMasterACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::passiveReceiveSlave(const uint8_t& byte) {
  passiveSlave_.push_back(byte);

  if (passiveSlave_.size() == 1) passiveSlaveDBx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) passiveSlaveDBx_++;

  // size() > NN + DBx + CRC
  if (passiveSlave_.size() >= 1 + passiveSlaveDBx_ + 1) {
    passiveTelegram_.createSlave(passiveSlave_);
    if (passiveTelegram_.getSlaveState() != SequenceState::seq_ok) {
      counter_.errorPassiveSlave++;
      callOnError("errorPassiveSlave", passiveMaster_.to_vector(),
                  passiveSlave_.to_vector());
    }
    state_ = HandlerState::passiveReceiveSlaveAcknowledge;
  }
}

void ebus::Handler::passiveReceiveSlaveAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::passive, TelegramType::master_slave,
                   passiveTelegram_.getMaster().to_vector(),
                   passiveTelegram_.getSlave().to_vector());
    counter_.messagesPassiveMasterSlave++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated_) {
    passiveSlaveRepeated_ = true;
    passiveSlave_.clear();
    passiveSlaveDBx_ = 0;
    state_ = HandlerState::passiveReceiveSlave;
  } else {
    counter_.errorPassiveSlaveACK++;
    callOnError("errorPassiveSlaveACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(const uint8_t& byte) {
  if (passiveTelegram_.getType() == TelegramType::master_master) {
    callOnTelegram(MessageType::reactive, TelegramType::master_master,
                   passiveTelegram_.getMaster().to_vector(),
                   passiveTelegram_.getSlave().to_vector());
    counter_.messagesReactiveMasterMaster++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else {
    callWrite(passiveSlave_[passiveSlaveIndex_]);
    state_ = HandlerState::reactiveSendSlave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(const uint8_t& byte) {
  state_ = HandlerState::passiveReceiveMaster;
  if (!passiveMasterRepeated_) {
    passiveMasterRepeated_ = true;
  } else {
    counter_.errorReactiveMasterACK++;
    callOnError("errorReactiveMasterACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
  }
}

void ebus::Handler::reactiveSendSlave(const uint8_t& byte) {
  passiveSlaveIndex_++;
  if (passiveSlaveIndex_ >= passiveSlave_.size())
    state_ = HandlerState::reactiveReceiveSlaveAcknowledge;
  else
    callWrite(passiveSlave_[passiveSlaveIndex_]);
}

void ebus::Handler::reactiveReceiveSlaveAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::reactive, TelegramType::master_slave,
                   passiveTelegram_.getMaster().to_vector(),
                   passiveTelegram_.getSlave().to_vector());
    counter_.messagesReactiveMasterSlave++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated_) {
    passiveSlaveRepeated_ = true;
    passiveSlaveIndex_ = 0;
    callWrite(passiveSlave_[passiveSlaveIndex_]);
    state_ = HandlerState::reactiveSendSlave;
  } else {
    counter_.errorReactiveSlaveACK++;
    callOnError("errorReactiveSlaveACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::requestBus(const uint8_t& byte) {
  auto won = [&]() {
    activeMaster = activeTelegram_.getMaster();
    activeMaster.push_back(activeTelegram_.getMasterCRC(), false);
    activeMaster.extend();
    if (activeMaster.size() > 1) {
      callOnBusRequestWon();
      activeMasterIndex_ = 1;
      callWrite(activeMaster[activeMasterIndex_]);
      state_ = HandlerState::activeSendMaster;
    } else {
      callOnBusRequestLost();
      counter_.resetActive00++;
      activeMessage_ = false;
      activeTelegram_.clear();
      activeMaster.clear();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    }
  };

  auto lost = [&]() {
    callOnBusRequestLost();
    passiveMaster_.push_back(byte);
    activeMessage_ = false;
    activeTelegram_.clear();
    activeMaster.clear();
    state_ = HandlerState::passiveReceiveMaster;
  };

  auto error = [&]() {
    callOnBusRequestLost();
    activeMessage_ = false;
    activeTelegram_.clear();
    activeMaster.clear();
    state_ = HandlerState::passiveReceiveMaster;
  };

  switch (request_->getResult()) {
    case RequestResult::observeSyn:
      error();
      break;
    case RequestResult::observeData:
      error();
      break;
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

void ebus::Handler::activeSendMaster(const uint8_t& byte) {
  activeMasterIndex_++;
  if (activeMasterIndex_ >= activeMaster.size()) {
    if (activeTelegram_.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     activeTelegram_.getMaster().to_vector(),
                     activeTelegram_.getSlave().to_vector());
      counter_.messagesActiveBroadcast++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    } else {
      state_ = HandlerState::activeReceiveMasterAcknowledge;
    }
  } else {
    callWrite(activeMaster[activeMasterIndex_]);
  }
}

void ebus::Handler::activeReceiveMasterAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    if (activeTelegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     activeTelegram_.getMaster().to_vector(),
                     activeTelegram_.getSlave().to_vector());
      counter_.messagesActiveMasterMaster++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    } else {
      state_ = HandlerState::activeReceiveSlave;
    }
  } else if (byte == sym_nak && !activeMasterRepeated_) {
    activeMasterRepeated_ = true;
    activeMasterIndex_ = 0;
    callWrite(activeMaster[activeMasterIndex_]);
    state_ = HandlerState::activeSendMaster;
  } else {
    counter_.errorActiveMasterACK++;
    if (activeMaster.size() == 6 && activeMaster[2] == 0x07 &&
        activeMaster[3] == 0x04)
      counter_.resetActive0704++;

    callOnError("errorActiveMasterACK", activeMaster.to_vector(),
                activeSlave_.to_vector());
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::releaseBus;
  }
}

void ebus::Handler::activeReceiveSlave(const uint8_t& byte) {
  activeSlave_.push_back(byte);

  if (activeSlave_.size() == 1) activeSlaveDBx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) activeSlaveDBx_++;

  // size() > NN + DBx + CRC
  if (activeSlave_.size() >= 1 + activeSlaveDBx_ + 1) {
    activeTelegram_.createSlave(activeSlave_);
    if (activeTelegram_.getSlaveState() == SequenceState::seq_ok) {
      callWrite(sym_ack);
      state_ = HandlerState::activeSendSlavePositiveAcknowledge;
    } else {
      counter_.errorActiveSlave++;
      callOnError("errorActiveSlave", activeMaster.to_vector(),
                  activeSlave_.to_vector());
      activeSlave_.clear();
      activeSlaveDBx_ = 0;
      callWrite(sym_nak);
      state_ = HandlerState::activeSendSlaveNegativeAcknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(const uint8_t& byte) {
  callOnTelegram(MessageType::active, TelegramType::master_slave,
                 activeTelegram_.getMaster().to_vector(),
                 activeTelegram_.getSlave().to_vector());
  counter_.messagesActiveMasterSlave++;
  callActiveReset();
  callWrite(sym_syn);
  state_ = HandlerState::releaseBus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(const uint8_t& byte) {
  if (!activeSlaveRepeated_) {
    activeSlaveRepeated_ = true;
    state_ = HandlerState::activeReceiveSlave;
  } else {
    counter_.errorActiveSlaveACK++;
    callOnError("errorActiveSlaveACK", activeMaster.to_vector(),
                activeSlave_.to_vector());
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::releaseBus;
  }
}

void ebus::Handler::releaseBus(const uint8_t& byte) {
  state_ = HandlerState::passiveReceiveMaster;
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
  if (passiveMaster_.size() > 0 || passiveSlave_.size() > 0) {
    callOnError("checkPassiveBuffers", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());

    if (passiveMaster_.size() == 1 && passiveMaster_[0] == 0x00)
      counter_.resetPassive00++;
    else
      counter_.resetPassive++;

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
  if (activeMaster.size() > 0 || activeSlave_.size() > 0) {
    callOnError("checkActiveBuffers", activeMaster.to_vector(),
                activeSlave_.to_vector());

    counter_.resetActive++;

    callActiveReset();
  }
}

void ebus::Handler::callPassiveReset() {
  passiveTelegram_.clear();

  passiveMaster_.clear();
  passiveMasterDBx_ = 0;
  passiveMasterRepeated_ = false;

  passiveSlave_.clear();
  passiveSlaveDBx_ = 0;
  passiveSlaveIndex_ = 0;
  passiveSlaveRepeated_ = false;
}

void ebus::Handler::callActiveReset() {
  activeMessage_ = false;
  activeTelegram_.clear();

  activeMaster.clear();
  activeMasterIndex_ = 0;
  activeMasterRepeated_ = false;

  activeSlave_.clear();
  activeSlaveDBx_ = 0;
  activeSlaveRepeated_ = false;
}

void ebus::Handler::callWrite(const uint8_t& byte) {
  if (bus_) {
    write_.markBegin();
    bus_->writeByte(byte);
    write_.markEnd();
  }
}

void ebus::Handler::callOnBusRequestWon() {
  if (busRequestWonCallback_) {
    callbackWon_.markBegin();
    busRequestWonCallback_();
    callbackWon_.markEnd();
  }
}

void ebus::Handler::callOnBusRequestLost() {
  if (busRequestLostCallback_) {
    callbackLost_.markBegin();
    busRequestLostCallback_();
    callbackLost_.markEnd();
  }
}

void ebus::Handler::callOnReactiveMasterSlave(
    const std::vector<uint8_t>& master, std::vector<uint8_t>* const slave) {
  if (reactiveMasterSlaveCallback_) {
    callbackReactive_.markBegin();
    reactiveMasterSlaveCallback_(master, slave);
    callbackReactive_.markEnd();
  }
}

void ebus::Handler::callOnTelegram(const MessageType& messageType,
                                   const TelegramType& telegramType,
                                   const std::vector<uint8_t>& master,
                                   const std::vector<uint8_t>& slave) {
  if (telegramCallback_) {
    callbackTelegram_.markBegin();
    telegramCallback_(messageType, telegramType, master, slave);
    callbackTelegram_.markEnd();
  }
}

void ebus::Handler::callOnError(const std::string& error,
                                const std::vector<uint8_t>& master,
                                const std::vector<uint8_t>& slave) {
  if (errorCallback_) {
    callbackError_.markBegin();
    errorCallback_(error, master, slave);
    callbackError_.markEnd();
  }
}
