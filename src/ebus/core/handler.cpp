/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/handler.hpp"

#include <ebus/utils.hpp>
#include <utility>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"

ebus::Handler::Handler(uint8_t source_address, Bus* bus, Request* request,
                       BusMonitor* monitor)
    : bus_(bus), request_(request), monitor_(monitor) {
  setSourceAddress(source_address);

  request_->setHandlerBusRequestedCallback([this]() {
    if (active_message_ && state_ != HandlerState::request_bus)
      state_ = HandlerState::request_bus;
  });

  request_->setStartBitCallback([this]() {
    if (active_message_) callActiveReset();
  });

  // Pre-allocate core buffers to avoid heap allocations in the hot path
  passive_master_.reserve(64);
  passive_slave_.reserve(64);
  active_master_.reserve(64);
  active_slave_.reserve(64);

  last_point_ = std::chrono::steady_clock::now();
}

void ebus::Handler::setSourceAddress(uint8_t source_address) {
  source_address_ =
      ebus::isMaster(source_address) ? source_address : DEFAULT_ADDRESS;
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

bool ebus::Handler::sendActiveMessage(ByteView message) {
  if (active_message_) return false;
  if (message.empty()) return false;

  active_telegram_.createMaster(source_address_, message);
  if (active_telegram_.getMasterState() == SequenceState::seq_ok) {
    active_message_ = true;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_master++; });
    callOnError("errorActiveMaster",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
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
      if (measure_sync_ && monitor_)
        monitor_->active_first.markEnd(ctx.timestamp);
      else if (monitor_)
        monitor_->active_data.markEnd(ctx.timestamp);
    } else {
      if (measure_sync_ && monitor_)
        monitor_->passive_first.markEnd(ctx.timestamp);
      else if (monitor_)
        monitor_->passive_data.markEnd(ctx.timestamp);
    }
    measure_sync_ = false;
  } else {
    if (measure_sync_ && monitor_) monitor_->sync.markEnd(ctx.timestamp);
    measure_sync_ = true;
  }

  last_point_ = ctx.timestamp;
  if (measure_sync_) {
    if (monitor_) {
      monitor_->sync.markBegin(last_point_);
      monitor_->active_first.markBegin(last_point_);
      monitor_->passive_first.markBegin(last_point_);
    }
  } else {
    if (monitor_) {
      monitor_->active_data.markBegin(last_point_);
      monitor_->passive_data.markBegin(last_point_);
    }
  }

  pending_write_.reset();

  size_t idx = static_cast<size_t>(state_);
  if (idx < NUM_HANDLER_STATES && kStateHandlers[idx]) {
    // Use a fresh "now" for the execution timing sample to measure CPU overhead
    auto exec_start = std::chrono::steady_clock::now();
    (this->*kStateHandlers[idx])(ctx.byte);  // handle byte

    if (monitor_) {
      monitor_->handler_timing[static_cast<size_t>(idx)].addSample(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - exec_start)
              .count());
    }
  }

  // Defer actual bus I/O until after the logic step
  if (pending_write_ && bus_) {
    if (monitor_) monitor_->write.markBegin();
    bus_->writeByte(*pending_write_);
    if (monitor_) monitor_->write.markEnd();
  }
}

void ebus::Handler::passiveReceiveMaster(uint8_t byte) {
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
                         {passive_telegram_.getMaster().data(),
                          passive_telegram_.getMaster().size()},
                         {passive_telegram_.getSlave().data(),
                          passive_telegram_.getSlave().size()});
          if (monitor_)
            monitor_->updateHandler(
                [](auto& m) { m.messages_passive_broadcast++; });
          callPassiveReset();
        } else if (passive_master_[1] == source_address_) {
          callWrite(sym_ack);
          state_ = HandlerState::reactive_send_master_positive_acknowledge;
        } else if (passive_master_[1] == target_address_) {
          passive_slave_.clear();
          callOnReactiveMasterSlave(passive_telegram_.getMaster(),
                                    passive_slave_);

          passive_telegram_.createSlave(passive_slave_);
          if (passive_telegram_.getSlaveState() == SequenceState::seq_ok) {
            passive_slave_ = passive_telegram_.getSlave();
            passive_slave_.pushBack(passive_telegram_.getSlaveCRC(), false);
            passive_slave_.extend();
            callWrite(sym_ack);
            state_ = HandlerState::reactive_send_master_positive_acknowledge;
          } else {
            if (monitor_)
              monitor_->updateHandler(
                  [](auto& m) { m.error_reactive_slave++; });
            callOnError("errorReactiveSlave",
                        {passive_master_.data(), passive_master_.size()},
                        {passive_slave_.data(), passive_slave_.size()});
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
          if (monitor_)
            monitor_->updateHandler([](auto& m) { m.error_reactive_master++; });
          callOnError("errorReactiveMaster",
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
          passive_telegram_.clear();
          passive_master_.clear();
          passive_master_dbx_ = 0;
          callWrite(sym_nak);
          state_ = HandlerState::reactive_send_master_negative_acknowledge;
        } else if (passive_telegram_.getType() == TelegramType::master_master ||
                   passive_telegram_.getType() == TelegramType::master_slave) {
          state_ = HandlerState::passive_receive_master_acknowledge;
        } else {
          if (monitor_)
            monitor_->updateHandler([](auto& m) { m.error_passive_master++; });
          callOnError("errorPassiveMaster",
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
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

void ebus::Handler::passiveReceiveMasterAcknowledge(uint8_t byte) {
  if (byte == sym_ack) {
    if (passive_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::passive, TelegramType::master_master,
                     {passive_telegram_.getMaster().data(),
                      passive_telegram_.getMaster().size()},
                     {passive_telegram_.getSlave().data(),
                      passive_telegram_.getSlave().size()});
      if (monitor_)
        monitor_->updateHandler(
            [](auto& m) { m.messages_passive_master_master++; });
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
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_passive_master_ack++; });
    if (passive_master_.size() == 6 && passive_master_[2] == 0x07 &&
        passive_master_[3] == 0x04) {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.reset_passive_0704++; });
    }

    callOnError("errorPassiveMasterACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::passiveReceiveSlave(uint8_t byte) {
  passive_slave_.pushBack(byte);

  if (passive_slave_.size() == 1) passive_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == sym_ext) passive_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (passive_slave_.size() >= 1 + passive_slave_dbx_ + 1) {
    passive_telegram_.createSlave(passive_slave_);
    if (passive_telegram_.getSlaveState() != SequenceState::seq_ok) {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.error_passive_slave++; });
      callOnError("errorPassiveSlave",
                  {passive_master_.data(), passive_master_.size()},
                  {passive_slave_.data(), passive_slave_.size()});
    }
    state_ = HandlerState::passive_receive_slave_acknowledge;
  }
}

void ebus::Handler::passiveReceiveSlaveAcknowledge(uint8_t byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::passive, TelegramType::master_slave,
                   {passive_telegram_.getMaster().data(),
                    passive_telegram_.getMaster().size()},
                   {passive_telegram_.getSlave().data(),
                    passive_telegram_.getSlave().size()});
    if (monitor_)
      monitor_->updateHandler(
          [](auto& m) { m.messages_passive_master_slave++; });
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else if (byte == sym_nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_.clear();
    passive_slave_dbx_ = 0;
    state_ = HandlerState::passive_receive_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_passive_slave_ack++; });
    callOnError("errorPassiveSlaveACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::reactiveSendMasterPositiveAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  if (passive_telegram_.getType() == TelegramType::master_master) {
    callOnTelegram(MessageType::reactive, TelegramType::master_master,
                   {passive_telegram_.getMaster().data(),
                    passive_telegram_.getMaster().size()},
                   {passive_telegram_.getSlave().data(),
                    passive_telegram_.getSlave().size()});
    if (monitor_)
      monitor_->updateHandler(
          [](auto& m) { m.messages_reactive_master_master++; });
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else {
    callWrite(passive_slave_[passive_slave_index_]);
    state_ = HandlerState::reactive_send_slave;
  }
}

void ebus::Handler::reactiveSendMasterNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  state_ = HandlerState::passive_receive_master;
  if (!passive_master_repeated_) {
    passive_master_repeated_ = true;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_reactive_master_ack++; });
    callOnError("errorReactiveMasterACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
  }
}

void ebus::Handler::reactiveSendSlave([[maybe_unused]] uint8_t byte) {
  passive_slave_index_++;
  if (passive_slave_index_ >= passive_slave_.size())
    state_ = HandlerState::reactive_receive_slave_acknowledge;
  else
    callWrite(passive_slave_[passive_slave_index_]);
}

void ebus::Handler::reactiveReceiveSlaveAcknowledge(uint8_t byte) {
  if (byte == sym_ack) {
    callOnTelegram(MessageType::reactive, TelegramType::master_slave,
                   {passive_telegram_.getMaster().data(),
                    passive_telegram_.getMaster().size()},
                   {passive_telegram_.getSlave().data(),
                    passive_telegram_.getSlave().size()});
    if (monitor_)
      monitor_->updateHandler(
          [](auto& m) { m.messages_reactive_master_slave++; });
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else if (byte == sym_nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_index_ = 0;
    callWrite(passive_slave_[passive_slave_index_]);
    state_ = HandlerState::reactive_send_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_reactive_slave_ack++; });
    callOnError("errorReactiveSlaveACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void ebus::Handler::requestBus(uint8_t byte) {
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
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.reset_active_00++; });
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

void ebus::Handler::activeSendMaster(uint8_t byte) {
  // Verify that the byte we just read from the bus matches what we sent
  // (Echo check). The index hasn't been incremented yet, so it points to
  // the byte we sent in the previous step.
  // If the check fails, we abort immediately to prevent bus contention.
  if (byte != active_master_[active_master_index_]) {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_master++; });
    callOnError("errorActiveMasterEcho",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();
    // Do not increment; abort and release bus or retry logic could trigger
    // here
  }

  active_master_index_++;
  if (active_master_index_ >= active_master_.size()) {
    if (active_telegram_.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     {active_master_.data(), active_master_.size()},
                     {active_slave_.data(), active_slave_.size()});

      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.messages_active_broadcast++; });
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

void ebus::Handler::activeReceiveMasterAcknowledge(uint8_t byte) {
  if (byte == sym_ack) {
    if (active_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     {active_master_.data(), active_master_.size()},
                     {active_slave_.data(), active_slave_.size()});
      if (monitor_)
        monitor_->updateHandler(
            [](auto& m) { m.messages_active_master_master++; });
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
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_master_ack++; });
    if (active_master_.size() == 6 && active_master_[2] == 0x07 &&
        active_master_[3] == 0x04) {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.reset_active_0704++; });
    }
    callOnError("errorActiveMasterACK",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::release_bus;
  }
}

void ebus::Handler::activeReceiveSlave(uint8_t byte) {
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
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.error_active_slave++; });
      callOnError("errorActiveSlave",
                  {active_master_.data(), active_master_.size()},
                  {active_slave_.data(), active_slave_.size()});
      active_slave_.clear();
      active_slave_dbx_ = 0;
      callWrite(sym_nak);
      state_ = HandlerState::active_send_slave_negative_acknowledge;
    }
  }
}

void ebus::Handler::activeSendSlavePositiveAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  callOnTelegram(
      MessageType::active, TelegramType::master_slave,
      {active_telegram_.getMaster().data(),
       active_telegram_.getMaster().size()},
      {active_telegram_.getSlave().data(), active_telegram_.getSlave().size()});
  if (monitor_)
    monitor_->updateHandler([](auto& m) { m.messages_active_master_slave++; });
  callActiveReset();
  callWrite(sym_syn);
  state_ = HandlerState::release_bus;
}

void ebus::Handler::activeSendSlaveNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  if (!active_slave_repeated_) {
    active_slave_repeated_ = true;
    state_ = HandlerState::active_receive_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_slave_ack++; });
    callOnError("errorActiveSlaveACK",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();
    callWrite(sym_syn);
    state_ = HandlerState::release_bus;
  }
}

void ebus::Handler::releaseBus([[maybe_unused]] uint8_t byte) {
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
    callOnError("checkPassiveBuffers",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});

    if (passive_master_.size() == 1 && passive_master_[0] == 0x00) {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.reset_passive_00++; });
    } else {
      if (monitor_) monitor_->updateHandler([](auto& m) { m.reset_passive++; });
    }

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
    callOnError("checkActiveBuffers",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});

    if (monitor_) monitor_->updateHandler([](auto& m) { m.reset_active++; });

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

void ebus::Handler::callWrite(uint8_t byte) { pending_write_ = byte; }

void ebus::Handler::callOnBusRequestWon() {
  if (bus_request_won_callback_) {
    if (monitor_) monitor_->callback_won.markBegin();
    bus_request_won_callback_();
    if (monitor_) monitor_->callback_won.markEnd();
  }
}

void ebus::Handler::callOnBusRequestLost() {
  if (bus_request_lost_callback_) {
    if (monitor_) monitor_->callback_lost.markBegin();
    bus_request_lost_callback_();
    if (monitor_) monitor_->callback_lost.markEnd();
  }
}

void ebus::Handler::callOnReactiveMasterSlave(ByteView master_view,
                                              Sequence& slave_response) {
  if (reactive_master_slave_callback_) {
    if (monitor_) monitor_->callback_reactive.markBegin();
    reactive_master_slave_callback_(master_view, slave_response);
    if (monitor_) monitor_->callback_reactive.markEnd();
  }
}

void ebus::Handler::callOnTelegram(MessageType message_type,
                                   TelegramType telegram_type,
                                   ByteView master_view, ByteView slave_view) {
  if (telegram_callback_) {
    if (monitor_) monitor_->callback_telegram.markBegin();
    telegram_callback_(message_type, telegram_type, master_view, slave_view);
    if (monitor_) monitor_->callback_telegram.markEnd();
  }
}

void ebus::Handler::callOnError(std::string_view error_message,
                                ByteView master_view, ByteView slave_view) {
  if (error_callback_) {
    if (monitor_) {
      monitor_->callback_error.markBegin();
      monitor_->updateBus([](auto& m) {
        m.last_error_timestamp = std::chrono::system_clock::now();
      });
    }
    error_callback_(error_message, last_result_, master_view, slave_view);
    if (monitor_) monitor_->callback_error.markEnd();
  }
}
