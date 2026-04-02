/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Core/Handler.hpp"

#include <utility>
#include <vector>

#include "Utils/Common.hpp"

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
    counter_.errorActiveMaster_++;
    callOnError("errorActiveMaster", activeMaster_.to_vector(),
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
        activeFirst_.markEnd();
      else
        activeData_.markEnd();
    } else {
      if (measureSync_)
        passiveFirst_.markEnd();
      else
        passiveData_.markEnd();
    }
    measureSync_ = false;
  } else {
    if (measureSync_) sync_.markEnd();
    measureSync_ = true;
  }

  lastPoint_ = std::chrono::steady_clock::now();
  if (measureSync_) {
    sync_.markBegin(lastPoint_);
    activeFirst_.markBegin(lastPoint_);
    passiveFirst_.markBegin(lastPoint_);
  } else {
    activeData_.markBegin(lastPoint_);
    passiveData_.markBegin(lastPoint_);
  }

  size_t idx = static_cast<size_t>(state_);
  if (idx < stateHandlers_.size() && stateHandlers_[idx]) {
    (this->*stateHandlers_[idx])(byte);  // handle byte
    if (byte != sym_syn || idx == static_cast<size_t>(HandlerState::releaseBus))
      handlerTiming_[static_cast<size_t>(idx)].addSample(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - lastPoint_)
              .count());
  }
}

void ebus::Handler::resetMetrics() {
#define X(name) counter_.name##_ = 0;
  EBUS_HANDLER_COUNTER_LIST
#undef X

#define X(name) name##_.reset();
  EBUS_HANDLER_TIMING_LIST
#undef X

  for (ebus::TimingStats& stats : handlerTiming_) {
    stats.reset();
  }
}

std::map<std::string, ebus::MetricValues> ebus::Handler::getMetrics() const {
  std::map<std::string, MetricValues> m;
  auto addCounter = [&](const std::string& name, uint32_t val) {
    m["handler.counter." + name] = {static_cast<double>(val),  0, 0, 0, 0,
                                    static_cast<uint64_t>(val)};
  };

  // 1. Calculate and map Counters
  Counter c = counter_;

  uint32_t msgTotal =
      c.messagesPassiveMasterSlave_ + c.messagesPassiveMasterMaster_ +
      c.messagesPassiveBroadcast_ + c.messagesActiveMasterSlave_ +
      c.messagesActiveMasterMaster_ + c.messagesActiveBroadcast_ +
      c.messagesReactiveMasterSlave_ + c.messagesReactiveMasterMaster_;
  addCounter("messagesTotal", msgTotal);

  uint32_t errPassive = c.errorPassiveMaster_ + c.errorPassiveMasterACK_ +
                        c.errorPassiveSlave_ + c.errorPassiveSlaveACK_;
  addCounter("errorPassive", errPassive);

  uint32_t errReactive = c.errorReactiveMaster_ + c.errorReactiveMasterACK_ +
                         c.errorReactiveSlave_ + c.errorReactiveSlaveACK_;
  addCounter("errorReactive", errReactive);

  uint32_t errActive = c.errorActiveMaster_ + c.errorActiveMasterACK_ +
                       c.errorActiveSlave_ + c.errorActiveSlaveACK_;
  addCounter("errorActive", errActive);

  uint32_t errTotal = errPassive + errReactive + errActive;
  addCounter("errorTotal", errTotal);

  // 2. Calculate Error Rate (%)
  if (msgTotal > 0) {
    double errorRate =
        (static_cast<double>(errTotal) / (msgTotal + errTotal)) * 100.0;
    m["handler.errorRate"] = {errorRate, errorRate, errorRate,
                              errorRate, 0.0,       1};
  } else {
    m["handler.errorRate"] = {0.0, 0.0, 0.0, 0.0, 0.0, 0};
  }

#define X(name) addCounter(#name, c.name##_);
  EBUS_HANDLER_COUNTER_LIST
#undef X

// 2. Map the explicit TimingStats members
#define X(name) m["handler.timing." #name] = name##_.getValues();
  EBUS_HANDLER_TIMING_LIST
#undef X

  // 3. Map the state-specific timing array
  for (size_t i = 0; i < handlerTiming_.size(); ++i) {
    auto state = static_cast<HandlerState>(i);
    m["handler.timing.state." + std::string(getHandlerStateText(state))] =
        handlerTiming_[i].getValues();
  }

  return m;
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
          counter_.messagesPassiveBroadcast_++;
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
            counter_.errorReactiveSlave_++;
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
          counter_.errorReactiveMaster_++;
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
          counter_.errorPassiveMaster_++;
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
      counter_.messagesPassiveMasterMaster_++;
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
    counter_.errorPassiveMasterACK_++;
    if (passiveMaster_.size() == 6 && passiveMaster_[2] == 0x07 &&
        passiveMaster_[3] == 0x04)
      counter_.resetPassive0704_++;

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
      counter_.errorPassiveSlave_++;
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
    counter_.messagesPassiveMasterSlave_++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated_) {
    passiveSlaveRepeated_ = true;
    passiveSlave_.clear();
    passiveSlaveDBx_ = 0;
    state_ = HandlerState::passiveReceiveSlave;
  } else {
    counter_.errorPassiveSlaveACK_++;
    callOnError("errorPassiveSlaveACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  if (passiveTelegram_.getType() == TelegramType::master_master) {
    callOnTelegram(MessageType::reactive, TelegramType::master_master,
                   passiveTelegram_.getMaster().to_vector(),
                   passiveTelegram_.getSlave().to_vector());
    counter_.messagesReactiveMasterMaster_++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else {
    callWrite(passiveSlave_[passiveSlaveIndex_]);
    state_ = HandlerState::reactiveSendSlave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  state_ = HandlerState::passiveReceiveMaster;
  if (!passiveMasterRepeated_) {
    passiveMasterRepeated_ = true;
  } else {
    counter_.errorReactiveMasterACK_++;
    callOnError("errorReactiveMasterACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
  }
}

void ebus::Handler::reactiveSendSlave(const uint8_t& byte) {
  (void)byte;  // unused
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
    counter_.messagesReactiveMasterSlave_++;
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  } else if (byte == sym_nak && !passiveSlaveRepeated_) {
    passiveSlaveRepeated_ = true;
    passiveSlaveIndex_ = 0;
    callWrite(passiveSlave_[passiveSlaveIndex_]);
    state_ = HandlerState::reactiveSendSlave;
  } else {
    counter_.errorReactiveSlaveACK_++;
    callOnError("errorReactiveSlaveACK", passiveMaster_.to_vector(),
                passiveSlave_.to_vector());
    callPassiveReset();
    state_ = HandlerState::passiveReceiveMaster;
  }
}

void ebus::Handler::requestBus(const uint8_t& byte) {
  auto won = [&]() {
    activeMaster_ = activeTelegram_.getMaster();
    activeMaster_.push_back(activeTelegram_.getMasterCRC(), false);
    activeMaster_.extend();
    if (activeMaster_.size() > 1) {
      callOnBusRequestWon();
      activeMasterIndex_ = 1;
      callWrite(activeMaster_[activeMasterIndex_]);
      state_ = HandlerState::activeSendMaster;
    } else {
      callOnBusRequestLost();
      counter_.resetActive00_++;
      activeMessage_ = false;
      activeTelegram_.clear();
      activeMaster_.clear();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    }
  };

  auto lost = [&]() {
    callOnBusRequestLost();
    passiveMaster_.push_back(byte);
    activeMessage_ = false;
    activeTelegram_.clear();
    activeMaster_.clear();
    state_ = HandlerState::passiveReceiveMaster;
  };

  auto error = [&]() {
    callOnBusRequestLost();
    activeMessage_ = false;
    activeTelegram_.clear();
    activeMaster_.clear();
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
  // Verify that the byte we just read from the bus matches what we sent
  // (Echo check). The index hasn't been incremented yet, so it points to
  // the byte we sent in the previous step.
  // If the check fails, we abort immediately to prevent bus contention.
  if (byte != activeMaster_[activeMasterIndex_]) {
    counter_.errorActiveMaster_++;
    callOnError("errorActiveMasterEcho", activeMaster_.to_vector(),
                activeSlave_.to_vector());
    callActiveReset();
    // Do not increment; abort and release bus or retry logic could trigger here
  }

  activeMasterIndex_++;
  if (activeMasterIndex_ >= activeMaster_.size()) {
    if (activeTelegram_.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     activeTelegram_.getMaster().to_vector(),
                     activeTelegram_.getSlave().to_vector());
      counter_.messagesActiveBroadcast_++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    } else {
      state_ = HandlerState::activeReceiveMasterAcknowledge;
    }
  } else {
    callWrite(activeMaster_[activeMasterIndex_]);
  }
}

void ebus::Handler::activeReceiveMasterAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    if (activeTelegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     activeTelegram_.getMaster().to_vector(),
                     activeTelegram_.getSlave().to_vector());
      counter_.messagesActiveMasterMaster_++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::releaseBus;
    } else {
      state_ = HandlerState::activeReceiveSlave;
    }
  } else if (byte == sym_nak && !activeMasterRepeated_) {
    activeMasterRepeated_ = true;
    activeMasterIndex_ = 0;
    callWrite(activeMaster_[activeMasterIndex_]);
    state_ = HandlerState::activeSendMaster;
  } else {
    counter_.errorActiveMasterACK_++;
    if (activeMaster_.size() == 6 && activeMaster_[2] == 0x07 &&
        activeMaster_[3] == 0x04)
      counter_.resetActive0704_++;

    callOnError("errorActiveMasterACK", activeMaster_.to_vector(),
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
      counter_.errorActiveSlave_++;
      callOnError("errorActiveSlave", activeMaster_.to_vector(),
                  activeSlave_.to_vector());
      activeSlave_.clear();
      activeSlaveDBx_ = 0;
      callWrite(sym_nak);
      state_ = HandlerState::activeSendSlaveNegativeAcknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  callOnTelegram(MessageType::active, TelegramType::master_slave,
                 activeTelegram_.getMaster().to_vector(),
                 activeTelegram_.getSlave().to_vector());
  counter_.messagesActiveMasterSlave_++;
  callActiveReset();
  callWrite(sym_syn);
  state_ = HandlerState::releaseBus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  if (!activeSlaveRepeated_) {
    activeSlaveRepeated_ = true;
    state_ = HandlerState::activeReceiveSlave;
  } else {
    counter_.errorActiveSlaveACK_++;
    callOnError("errorActiveSlaveACK", activeMaster_.to_vector(),
                activeSlave_.to_vector());
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::releaseBus;
  }
}

void ebus::Handler::releaseBus(const uint8_t& byte) {
  (void)byte;  // unused
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
      counter_.resetPassive00_++;
    else
      counter_.resetPassive_++;

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
  if (activeMaster_.size() > 0 || activeSlave_.size() > 0) {
    callOnError("checkActiveBuffers", activeMaster_.to_vector(),
                activeSlave_.to_vector());

    counter_.resetActive_++;

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

  activeMaster_.clear();
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
