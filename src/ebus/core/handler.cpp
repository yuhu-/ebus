/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/handler.hpp"

#include <ebus/utils.hpp>
#include <utility>

ebus::Handler::Handler(const uint8_t& address, Bus* bus, Request* request)
    : bus_(bus), request_(request) {
  setSourceAddress(address);

  request_->setHandlerBusRequestedCallback([this]() {
    if (active_message_ && state_ != HandlerState::request_bus)
      state_ = HandlerState::request_bus;
  });

  request_->setStartBitCallback([this]() {
    if (active_message_) callActiveReset();
  });

  state_handlers_ = {&Handler::passiveReceiveMaster,
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

  // Pre-allocate core buffers to avoid heap allocations in the hot path
  passive_master_.reserve(64);
  passive_slave_.reserve(64);
  active_master_.reserve(64);
  active_slave_.reserve(64);

  last_point_ = std::chrono::steady_clock::now();
}

void ebus::Handler::setSourceAddress(const uint8_t& address) {
  source_address_ = ebus::isMaster(address) ? address : DEFAULT_ADDRESS;
  target_address_ = slaveOf(source_address_);
}

uint8_t ebus::Handler::getSourceAddress() const { return source_address_; }

uint8_t ebus::Handler::getTargetAddress() const { return target_address_; }

void ebus::Handler::setBusRequestWonCallback(BusRequestWonCallback callback) {
  bus_request_won_callback_ = std::move(callback);
}

void ebus::Handler::setBusRequestLostCallback(BusRequestLostCallback callback) {
  bus_request_lost_callback_ = std::move(callback);
}

void ebus::Handler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  reactive_master_slave_callback_ = std::move(callback);
}

void ebus::Handler::setTelegramCallback(TelegramCallback callback) {
  telegram_callback_ = std::move(callback);
}

void ebus::Handler::setErrorCallback(ErrorCallback callback) {
  error_callback_ = std::move(callback);
}

ebus::HandlerState ebus::Handler::getState() const { return state_; }

bool ebus::Handler::sendActiveMessage(const std::vector<uint8_t>& message) {
  if (active_message_) return false;
  if (message.empty()) return false;

  active_telegram_.createMaster(source_address_, message);
  if (active_telegram_.getMasterState() == SequenceState::seq_ok) {
    active_message_ = true;
  } else {
    counter_.error_active_master_++;
    callOnError("errorActiveMaster", active_master_.toVector(),
                active_slave_.toVector());
  }

  return active_message_;
}

bool ebus::Handler::isActiveMessagePending() const { return active_message_; }

void ebus::Handler::reset() {
  state_ = HandlerState::passive_receive_master;
  callActiveReset();
  callPassiveReset();
}

void ebus::Handler::run(const BusEventContext& ctx) {
  last_result_ = ctx.result;
  // record timing
  if (ctx.byte != sym_syn) {
    if (active_message_) {
      if (measure_sync_)
        active_first_.markEnd(ctx.timestamp);
      else
        active_data_.markEnd(ctx.timestamp);
    } else {
      if (measure_sync_)
        passive_first_.markEnd(ctx.timestamp);
      else
        passive_data_.markEnd(ctx.timestamp);
    }
    measure_sync_ = false;
  } else {
    if (measure_sync_) sync_.markEnd(ctx.timestamp);
    measure_sync_ = true;
  }

  last_point_ = ctx.timestamp;
  if (measure_sync_) {
    sync_.markBegin(last_point_);
    active_first_.markBegin(last_point_);
    passive_first_.markBegin(last_point_);
  } else {
    active_data_.markBegin(last_point_);
    passive_data_.markBegin(last_point_);
  }

  size_t idx = static_cast<size_t>(state_);
  if (idx < state_handlers_.size() && state_handlers_[idx]) {
    // Use a fresh "now" for the execution timing sample to measure CPU overhead
    auto exec_start = std::chrono::steady_clock::now();
    (this->*state_handlers_[idx])(ctx.byte);  // handle byte
    if (ctx.byte != sym_syn ||
        idx == static_cast<size_t>(HandlerState::release_bus))
      handler_timing_[static_cast<size_t>(idx)].addSample(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - exec_start)
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

  for (ebus::TimingStats& stats : handler_timing_) {
    stats.reset();
  }
}

std::map<std::string, ebus::MetricValues> ebus::Handler::getMetrics() const {
  std::map<std::string, MetricValues> m;
  auto add_counter = [&](const std::string& name, uint32_t val) {
    m["handler.counter." + name] = {static_cast<double>(val),  0, 0, 0, 0,
                                    static_cast<uint64_t>(val)};
  };

  // 1. Calculate and map Counters
  Counter c = counter_;

  uint32_t msg_total =
      c.messages_passive_master_slave_ + c.messages_passive_master_master_ +
      c.messages_passive_broadcast_ + c.messages_active_master_slave_ +
      c.messages_active_master_master_ + c.messages_active_broadcast_ +
      c.messages_reactive_master_slave_ + c.messages_reactive_master_master_;
  add_counter("messages_total", msg_total);

  uint32_t err_passive = c.error_passive_master_ + c.error_passive_master_ack_ +
                         c.error_passive_slave_ + c.error_passive_slave_ack_;
  add_counter("error_passive", err_passive);

  uint32_t err_reactive = c.error_reactive_master_ +
                          c.error_reactive_master_ack_ +
                          c.error_reactive_slave_ + c.error_reactive_slave_ack_;
  add_counter("error_reactive", err_reactive);

  uint32_t err_active = c.error_active_master_ + c.error_active_master_ack_ +
                        c.error_active_slave_ + c.error_active_slave_ack_;
  add_counter("error_active", err_active);

  uint32_t err_total = err_passive + err_reactive + err_active;
  add_counter("error_total", err_total);

  // 2. Calculate Error Rate (%)
  if (msg_total > 0) {
    double error_rate =
        (static_cast<double>(err_total) / (msg_total + err_total)) * 100.0;
    m["handler.error_rate"] = {error_rate, error_rate, error_rate,
                               error_rate, 0.0,        1};
  } else {
    m["handler.error_rate"] = {0.0, 0.0, 0.0, 0.0, 0.0, 0};
  }

#define X(name) add_counter(#name, c.name##_);
  EBUS_HANDLER_COUNTER_LIST
#undef X

// 2. Map the explicit TimingStats members
#define X(name) m["handler.timing." #name] = name##_.getValues();
  EBUS_HANDLER_TIMING_LIST
#undef X

  // 3. Map the state-specific timing array
  for (size_t i = 0; i < handler_timing_.size(); ++i) {
    auto state = static_cast<HandlerState>(i);
    m["handler.timing.state." + std::string(getHandlerStateText(state))] =
        handler_timing_[i].getValues();
  }

  return m;
}

void ebus::Handler::passiveReceiveMaster(const uint8_t& byte) {
  if (byte != sym_syn) {
    passive_master_.pushBack(byte);

    if (passive_master_.size() == 5) passive_master_dbx_ = passive_master_[4];

    // AA >> A9 + 01 || A9 >> A9 + 00
    if (byte == sym_ext) passive_master_dbx_++;

    // size() > ZZ QQ PB SB NN + DBx + CRC
    if (passive_master_.size() >= 5 + passive_master_dbx_ + 1) {
      passive_telegram_.createMaster(passive_master_);
      if (passive_telegram_.getMasterState() == SequenceState::seq_ok) {
        if (passive_telegram_.getType() == TelegramType::broadcast) {
          callOnTelegram(MessageType::passive, TelegramType::broadcast,
                         passive_telegram_.getMaster().toVector(),
                         passive_telegram_.getSlave().toVector());
          counter_.messages_passive_broadcast_++;
          callPassiveReset();
        } else if (passive_master_[1] == source_address_) {
          callWrite(sym_ack);
          state_ = HandlerState::reactive_send_master_positive_acknowledge;
        } else if (passive_master_[1] == target_address_) {
          std::vector<uint8_t> response;
          callOnReactiveMasterSlave(passive_telegram_.getMaster().toVector(),
                                    &response);
          passive_telegram_.createSlave(response);
          if (passive_telegram_.getSlaveState() == SequenceState::seq_ok) {
            passive_slave_ = passive_telegram_.getSlave();
            passive_slave_.pushBack(passive_telegram_.getSlaveCRC(), false);
            passive_slave_.extend();
            callWrite(sym_ack);
            state_ = HandlerState::reactive_send_master_positive_acknowledge;
          } else {
            counter_.error_reactive_slave_++;
            callOnError("errorReactiveSlave", passive_master_.toVector(),
                        passive_slave_.toVector());
            callPassiveReset();
            callWrite(sym_syn);
            state_ = HandlerState::release_bus;
          }
        } else {
          state_ = HandlerState::passive_receive_master_acknowledge;
        }
      } else {
        if (passive_master_[1] == source_address_ ||
            passive_master_[1] == target_address_) {
          counter_.error_reactive_master_++;
          callOnError("errorReactiveMaster", passive_master_.toVector(),
                      passive_slave_.toVector());
          passive_telegram_.clear();
          passive_master_.clear();
          passive_master_dbx_ = 0;
          callWrite(sym_nak);
          state_ = HandlerState::reactive_send_master_negative_acknowledge;
        } else if (passive_telegram_.getType() == TelegramType::master_master ||
                   passive_telegram_.getType() == TelegramType::master_slave) {
          state_ = HandlerState::passive_receive_master_acknowledge;
        } else {
          counter_.error_passive_master_++;
          callOnError("errorPassiveMaster", passive_master_.toVector(),
                      passive_slave_.toVector());
          callPassiveReset();
        }
      }
    }
  } else {
    checkPassiveBuffers();
    checkActiveBuffers();

    // Initiate request bus
    if (active_message_) request_->requestBus(source_address_);
  }
}

void ebus::Handler::passiveReceiveMasterAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    if (passive_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::passive, TelegramType::master_master,
                     passive_telegram_.getMaster().toVector(),
                     passive_telegram_.getSlave().toVector());
      counter_.messages_passive_master_master_++;
      callPassiveReset();
      state_ = HandlerState::passive_receive_master;
    } else {
      state_ = HandlerState::passive_receive_slave;
    }
  } else if (byte != sym_syn && !passive_master_repeated_) {
    passive_master_repeated_ = true;
    passive_telegram_.clear();
    passive_master_.clear();
    passive_master_dbx_ = 0;
    state_ = HandlerState::passive_receive_master;
  } else {
    counter_.error_passive_master_ack_++;
    if (passive_master_.size() == 6 && passive_master_[2] == 0x07 &&
        passive_master_[3] == 0x04)
      counter_.reset_passive_0704_++;

    callOnError("errorPassiveMasterACK", passive_master_.toVector(),
                passive_slave_.toVector());
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::passiveReceiveSlave(const uint8_t& byte) {
  passive_slave_.pushBack(byte);

  if (passive_slave_.size() == 1) passive_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) passive_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (passive_slave_.size() >= 1 + passive_slave_dbx_ + 1) {
    passive_telegram_.createSlave(passive_slave_);
    if (passive_telegram_.getSlaveState() != SequenceState::seq_ok) {
      counter_.error_passive_slave_++;
      callOnError("errorPassiveSlave", passive_master_.toVector(),
                  passive_slave_.toVector());
    }
    state_ = HandlerState::passive_receive_slave_acknowledge;
  }
}

void ebus::Handler::passiveReceiveSlaveAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::passive, TelegramType::master_slave,
                   passive_telegram_.getMaster().toVector(),
                   passive_telegram_.getSlave().toVector());
    counter_.messages_passive_master_slave_++;
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else if (byte == sym_nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_.clear();
    passive_slave_dbx_ = 0;
    state_ = HandlerState::passive_receive_slave;
  } else {
    counter_.error_passive_slave_ack_++;
    callOnError("errorPassiveSlaveACK", passive_master_.toVector(),
                passive_slave_.toVector());
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  if (passive_telegram_.getType() == TelegramType::master_master) {
    callOnTelegram(MessageType::reactive, TelegramType::master_master,
                   passive_telegram_.getMaster().toVector(),
                   passive_telegram_.getSlave().toVector());
    counter_.messages_reactive_master_master_++;
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else {
    callWrite(passive_slave_[passive_slave_index_]);
    state_ = HandlerState::reactive_send_slave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  state_ = HandlerState::passive_receive_master;
  if (!passive_master_repeated_) {
    passive_master_repeated_ = true;
  } else {
    counter_.error_reactive_master_ack_++;
    callOnError("errorReactiveMasterACK", passive_master_.toVector(),
                passive_slave_.toVector());
    callPassiveReset();
  }
}

void ebus::Handler::reactiveSendSlave(const uint8_t& byte) {
  (void)byte;  // unused
  passive_slave_index_++;
  if (passive_slave_index_ >= passive_slave_.size())
    state_ = HandlerState::reactive_receive_slave_acknowledge;
  else
    callWrite(passive_slave_[passive_slave_index_]);
}

void ebus::Handler::reactiveReceiveSlaveAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::reactive, TelegramType::master_slave,
                   passive_telegram_.getMaster().toVector(),
                   passive_telegram_.getSlave().toVector());
    counter_.messages_reactive_master_slave_++;
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else if (byte == sym_nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_index_ = 0;
    callWrite(passive_slave_[passive_slave_index_]);
    state_ = HandlerState::reactive_send_slave;
  } else {
    counter_.error_reactive_slave_ack_++;
    callOnError("errorReactiveSlaveACK", passive_master_.toVector(),
                passive_slave_.toVector());
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::requestBus(const uint8_t& byte) {
  auto won = [&]() {
    active_master_ = active_telegram_.getMaster();
    active_master_.pushBack(active_telegram_.getMasterCRC(), false);
    active_master_.extend();
    if (active_master_.size() > 1) {
      callOnBusRequestWon();
      active_master_index_ = 1;
      callWrite(active_master_[active_master_index_]);
      state_ = HandlerState::active_send_master;
    } else {
      callOnBusRequestLost();
      counter_.reset_active_00_++;
      active_message_ = false;
      active_telegram_.clear();
      active_master_.clear();
      callWrite(sym_syn);
      state_ = HandlerState::release_bus;
    }
  };

  auto lost = [&]() {
    callOnBusRequestLost();
    passive_master_.pushBack(byte);
    active_message_ = false;
    active_telegram_.clear();
    active_master_.clear();
    state_ = HandlerState::passive_receive_master;
  };

  auto error = [&]() {
    callOnBusRequestLost();
    active_message_ = false;
    active_telegram_.clear();
    active_master_.clear();
    state_ = HandlerState::passive_receive_master;
  };

  switch (last_result_) {
    case RequestResult::observe_syn:
      error();
      break;
    case RequestResult::observe_data:
      error();
      break;
    case RequestResult::first_syn:
      break;
    case RequestResult::first_won:
      won();
      break;
    case RequestResult::first_retry:
      break;
    case RequestResult::first_lost:
      lost();
      break;
    case RequestResult::first_error:
      lost();
      break;
    case RequestResult::retry_syn:
      break;
    case RequestResult::retry_error:
      error();
      break;
    case RequestResult::second_won:
      won();
      break;
    case RequestResult::second_lost:
      lost();
      break;
    case RequestResult::second_error:
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
  if (byte != active_master_[active_master_index_]) {
    counter_.error_active_master_++;
    callOnError("errorActiveMasterEcho", active_master_.toVector(),
                active_slave_.toVector());
    callActiveReset();
    // Do not increment; abort and release bus or retry logic could trigger here
  }

  active_master_index_++;
  if (active_master_index_ >= active_master_.size()) {
    if (active_telegram_.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     active_telegram_.getMaster().toVector(),
                     active_telegram_.getSlave().toVector());
      counter_.messages_active_broadcast_++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::release_bus;
    } else {
      state_ = HandlerState::active_receive_master_acknowledge;
    }
  } else {
    callWrite(active_master_[active_master_index_]);
  }
}

void ebus::Handler::activeReceiveMasterAcknowledge(const uint8_t& byte) {
  if (byte == sym_ack) {
    if (active_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     active_telegram_.getMaster().toVector(),
                     active_telegram_.getSlave().toVector());
      counter_.messages_active_master_master_++;
      callActiveReset();
      callWrite(sym_syn);
      state_ = HandlerState::release_bus;
    } else {
      state_ = HandlerState::active_receive_slave;
    }
  } else if (byte == sym_nak && !active_master_repeated_) {
    active_master_repeated_ = true;
    active_master_index_ = 0;
    callWrite(active_master_[active_master_index_]);
    state_ = HandlerState::active_send_master;
  } else {
    counter_.error_active_master_ack_++;
    if (active_master_.size() == 6 && active_master_[2] == 0x07 &&
        active_master_[3] == 0x04)
      counter_.reset_active_0704_++;

    callOnError("errorActiveMasterACK", active_master_.toVector(),
                active_slave_.toVector());
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::release_bus;
  }
}

void ebus::Handler::activeReceiveSlave(const uint8_t& byte) {
  active_slave_.pushBack(byte);

  if (active_slave_.size() == 1) active_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) active_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (active_slave_.size() >= 1 + active_slave_dbx_ + 1) {
    active_telegram_.createSlave(active_slave_);
    if (active_telegram_.getSlaveState() == SequenceState::seq_ok) {
      callWrite(sym_ack);
      state_ = HandlerState::active_send_slave_positive_acknowledge;
    } else {
      counter_.error_active_slave_++;
      callOnError("errorActiveSlave", active_master_.toVector(),
                  active_slave_.toVector());
      active_slave_.clear();
      active_slave_dbx_ = 0;
      callWrite(sym_nak);
      state_ = HandlerState::active_send_slave_negative_acknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  callOnTelegram(MessageType::active, TelegramType::master_slave,
                 active_telegram_.getMaster().toVector(),
                 active_telegram_.getSlave().toVector());
  counter_.messages_active_master_slave_++;
  callActiveReset();
  callWrite(sym_syn);
  state_ = HandlerState::release_bus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(const uint8_t& byte) {
  (void)byte;  // unused
  if (!active_slave_repeated_) {
    active_slave_repeated_ = true;
    state_ = HandlerState::active_receive_slave;
  } else {
    counter_.error_active_slave_ack_++;
    callOnError("errorActiveSlaveACK", active_master_.toVector(),
                active_slave_.toVector());
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::release_bus;
  }
}

void ebus::Handler::releaseBus(const uint8_t& byte) {
  (void)byte;  // unused
  state_ = HandlerState::passive_receive_master;
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
  if (passive_master_.size() > 0 || passive_slave_.size() > 0) {
    callOnError("checkPassiveBuffers", passive_master_.toVector(),
                passive_slave_.toVector());

    if (passive_master_.size() == 1 && passive_master_[0] == 0x00)
      counter_.reset_passive_00_++;
    else
      counter_.reset_passive_++;

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
  if (active_master_.size() > 0 || active_slave_.size() > 0) {
    callOnError("checkActiveBuffers", active_master_.toVector(),
                active_slave_.toVector());

    counter_.reset_active_++;

    callActiveReset();
  }
}

void ebus::Handler::callPassiveReset() {
  passive_telegram_.clear();

  passive_master_.clear();
  passive_master_dbx_ = 0;
  passive_master_repeated_ = false;

  passive_slave_.clear();
  passive_slave_dbx_ = 0;
  passive_slave_index_ = 0;
  passive_slave_repeated_ = false;
}

void ebus::Handler::callActiveReset() {
  active_message_ = false;
  active_telegram_.clear();

  active_master_.clear();
  active_master_index_ = 0;
  active_master_repeated_ = false;

  active_slave_.clear();
  active_slave_dbx_ = 0;
  active_slave_repeated_ = false;
}

void ebus::Handler::callWrite(const uint8_t& byte) {
  if (bus_) {
    write_.markBegin();
    bus_->writeByte(byte);
    write_.markEnd();
  }
}

void ebus::Handler::callOnBusRequestWon() {
  if (bus_request_won_callback_) {
    callback_won_.markBegin();
    bus_request_won_callback_();
    callback_won_.markEnd();
  }
}

void ebus::Handler::callOnBusRequestLost() {
  if (bus_request_lost_callback_) {
    callback_lost_.markBegin();
    bus_request_lost_callback_();
    callback_lost_.markEnd();
  }
}

void ebus::Handler::callOnReactiveMasterSlave(
    const std::vector<uint8_t>& master, std::vector<uint8_t>* const slave) {
  if (reactive_master_slave_callback_) {
    callback_reactive_.markBegin();
    reactive_master_slave_callback_(master, slave);
    callback_reactive_.markEnd();
  }
}

void ebus::Handler::callOnTelegram(const MessageType& message_type,
                                   const TelegramType& telegram_type,
                                   const std::vector<uint8_t>& master,
                                   const std::vector<uint8_t>& slave) {
  if (telegram_callback_) {
    callback_telegram_.markBegin();
    telegram_callback_(message_type, telegram_type, master, slave);
    callback_telegram_.markEnd();
  }
}

void ebus::Handler::callOnError(const std::string& error_message,
                                const std::vector<uint8_t>& master,
                                const std::vector<uint8_t>& slave) {
  if (error_callback_) {
    callback_error_.markBegin();
    error_callback_(error_message, master, slave);
    callback_error_.markEnd();
  }
}
