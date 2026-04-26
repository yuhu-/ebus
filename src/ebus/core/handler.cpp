/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/handler.hpp"

#include <ebus/utils.hpp>
#include <utility>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"

namespace ebus::detail {

Handler::Handler(uint8_t source_address, Bus* bus, detail::Request* request,
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

void Handler::setSourceAddress(uint8_t source_address) {
  source_address_ = ebus::isMaster(source_address)
                        ? source_address
                        : ebus::RuntimeConfig{}.address;
  target_address_ = slaveOf(source_address_);
}

uint8_t Handler::getSourceAddress() const { return source_address_; }

uint8_t Handler::getTargetAddress() const { return target_address_; }

void Handler::setBusRequestWonCallback(BusRequestWonCallback callback) {
  bus_request_won_callback_ = std::move(callback);
}

void Handler::setBusRequestLostCallback(BusRequestLostCallback callback) {
  bus_request_lost_callback_ = std::move(callback);
}

void Handler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  reactive_master_slave_callback_ = std::move(callback);
}

void Handler::setTelegramCallback(TelegramCallback callback) {
  telegram_callback_ = std::move(callback);
}

void Handler::setErrorCallback(ErrorCallback callback) {
  error_callback_ = std::move(callback);
}

ebus::HandlerState Handler::getState() const { return state_; }

bool Handler::sendActiveMessage(ByteView message) {
  if (active_message_) return false;
  if (message.empty()) return false;

  active_telegram_.createMaster(source_address_, message);
  if (active_telegram_.getMasterState() == SequenceState::seq_ok) {
    active_message_ = true;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_master++; });
    callOnError(LogLevel::error, "errorActiveMaster",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
  }

  return active_message_;
}

bool Handler::isActiveMessagePending() const { return active_message_; }

void Handler::reset() {
  state_ = HandlerState::passive_receive_master;
  callActiveReset();
  callPassiveReset();
}

void Handler::run(const BusEventContext& ctx) {
  last_result_ = ctx.result;
  // record timing
  if (ctx.byte != Symbols::syn) {
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
  if (idx < FsmLimits::num_handler_states && kStateHandlers[idx]) {
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

void Handler::passiveReceiveMaster(uint8_t byte) {
  if (byte != Symbols::syn) {
    passive_master_.pushBack(byte);

    if (passive_master_.size() == 5) passive_master_dbx_ = passive_master_[4];

    // AA >> A9 + 01 || A9 >> A9 + 00
    if (byte == Symbols::ext) passive_master_dbx_++;

    // size() > ZZ QQ PB SB NN + DBx + CRC
    if (passive_master_.size() >=
        5 + passive_master_dbx_ + 1) {  // 5 bytes header + data + CRC
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
          callWrite(Symbols::ack);
          state_ = HandlerState::reactive_send_master_positive_acknowledge;
        } else if (passive_master_[1] == target_address_) {
          passive_slave_.clear();
          callOnReactiveMasterSlave(passive_telegram_.getMaster(),
                                    passive_slave_);

          passive_telegram_.createSlave(passive_slave_);
          if (passive_telegram_.getSlaveState() == SequenceState::seq_ok) {
            passive_slave_ =
                passive_telegram_.getSlave();  // Copy the slave response
            passive_slave_.pushBack(passive_telegram_.getSlaveCRC(), false);
            passive_slave_.extend();
            callWrite(Symbols::ack);
            state_ = HandlerState::reactive_send_master_positive_acknowledge;
          } else {
            if (monitor_)
              monitor_->updateHandler(
                  [](auto& m) { m.error_reactive_slave++; });
            callOnError(LogLevel::error, "errorReactiveSlave",
                        {passive_master_.data(), passive_master_.size()},
                        {passive_slave_.data(), passive_slave_.size()});
            callPassiveReset();
            callWrite(Symbols::syn);
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
          callOnError(LogLevel::error, "errorReactiveMaster",
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
          passive_telegram_.clear();
          passive_master_.clear();
          passive_master_dbx_ = 0;
          callWrite(Symbols::nak);
          state_ = HandlerState::reactive_send_master_negative_acknowledge;
        } else if (passive_telegram_.getType() == TelegramType::master_master ||
                   passive_telegram_.getType() == TelegramType::master_slave) {
          state_ = HandlerState::passive_receive_master_acknowledge;
        } else {
          if (monitor_)
            monitor_->updateHandler(
                [](auto& m) { m.error_passive_master++; });  // Protocol error
          callOnError(LogLevel::error, "errorPassiveMaster",
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
          callPassiveReset();
        }
      }
    }
  } else {  // Received SYN
    checkPassiveBuffers();
    checkActiveBuffers();

    // Initiate request bus
    if (active_message_) request_->requestBus(source_address_);
  }
}

void Handler::passiveReceiveMasterAcknowledge(uint8_t byte) {
  if (byte == Symbols::ack) {
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
  } else if (byte != Symbols::syn && !passive_master_repeated_) {
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

    callOnError(LogLevel::error, "errorPassiveMasterACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void Handler::passiveReceiveSlave(uint8_t byte) {
  passive_slave_.pushBack(byte);

  if (passive_slave_.size() == 1) passive_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == Symbols::ext) passive_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (passive_slave_.size() >=
      1 + passive_slave_dbx_ + 1) {  // 1 byte NN + data + CRC
    passive_telegram_.createSlave(passive_slave_);
    if (passive_telegram_.getSlaveState() != SequenceState::seq_ok) {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.error_passive_slave++; });
      callOnError(LogLevel::error, "errorPassiveSlave",
                  {passive_master_.data(), passive_master_.size()},
                  {passive_slave_.data(), passive_slave_.size()});
    }
    state_ = HandlerState::passive_receive_slave_acknowledge;
  }
}

void Handler::passiveReceiveSlaveAcknowledge(uint8_t byte) {
  if (byte == Symbols::ack) {
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
  } else if (byte == Symbols::nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_.clear();
    passive_slave_dbx_ = 0;
    state_ = HandlerState::passive_receive_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_passive_slave_ack++; });
    callOnError(LogLevel::error, "errorPassiveSlaveACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void Handler::reactiveSendMasterPositiveAcknowledge(
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
    callWrite(passive_slave_[passive_slave_index_]);  // Send next slave byte
    state_ = HandlerState::reactive_send_slave;
  }
}

void Handler::reactiveSendMasterNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  state_ = HandlerState::passive_receive_master;
  if (!passive_master_repeated_) {
    passive_master_repeated_ = true;  // Allow one retry
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_reactive_master_ack++; });
    callOnError(LogLevel::error, "errorReactiveMasterACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
  }
}

void Handler::reactiveSendSlave([[maybe_unused]] uint8_t byte) {
  passive_slave_index_++;
  if (passive_slave_index_ >= passive_slave_.size())  // All slave bytes sent
    state_ = HandlerState::reactive_receive_slave_acknowledge;
  else
    callWrite(passive_slave_[passive_slave_index_]);
}

void Handler::reactiveReceiveSlaveAcknowledge(uint8_t byte) {
  if (byte == Symbols::ack) {
    callOnTelegram(MessageType::reactive,
                   TelegramType::master_slave,  // Successful reactive response
                   {passive_telegram_.getMaster().data(),
                    passive_telegram_.getMaster().size()},
                   {passive_telegram_.getSlave().data(),
                    passive_telegram_.getSlave().size()});
    if (monitor_)
      monitor_->updateHandler(
          [](auto& m) { m.messages_reactive_master_slave++; });
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  } else if (byte == Symbols::nak &&
             !passive_slave_repeated_) {  // Negative Symbols::ack, retry slave
                                          // response
    passive_slave_repeated_ = true;
    passive_slave_index_ = 0;
    callWrite(passive_slave_[passive_slave_index_]);
    state_ = HandlerState::reactive_send_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_reactive_slave_ack++; });
    callOnError(LogLevel::error, "errorReactiveSlaveACK",
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    state_ = HandlerState::passive_receive_master;
  }
}

void Handler::requestBus(uint8_t byte) {
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
      active_telegram_.clear();  // Clear active message state
      active_master_.clear();
      callWrite(Symbols::syn);
      state_ = HandlerState::release_bus;
    }
  };

  auto lost = [&]() {
    callOnBusRequestLost();
    passive_master_.pushBack(byte);
    active_message_ = false;
    active_telegram_.clear();  // Clear active message state
    active_master_.clear();
    state_ = HandlerState::passive_receive_master;
  };

  auto error = [&]() {
    callOnBusRequestLost();
    active_message_ = false;
    active_telegram_.clear();  // Clear active message state
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

void Handler::activeSendMaster(uint8_t byte) {
  // Verify that the byte we just read from the bus matches what we sent
  // (Echo check). The index hasn't been incremented yet, so it points to
  // the byte we sent in the previous step.
  // If the check fails, we abort immediately to prevent bus contention.
  if (byte != active_master_[active_master_index_]) {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_master++; });
    callOnError(LogLevel::error, "errorActiveMasterEcho",
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
      callActiveReset();  // Reset active state
      callWrite(Symbols::syn);
      state_ = HandlerState::release_bus;
    } else {
      state_ = HandlerState::active_receive_master_acknowledge;
    }
  } else {
    callWrite(active_master_[active_master_index_]);
  }
}

void Handler::activeReceiveMasterAcknowledge(uint8_t byte) {
  if (byte == Symbols::ack) {
    if (active_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     {active_master_.data(), active_master_.size()},
                     {active_slave_.data(), active_slave_.size()});
      if (monitor_)
        monitor_->updateHandler(
            [](auto& m) { m.messages_active_master_master++; });
      callActiveReset();  // Reset active state
      callWrite(Symbols::syn);
      state_ = HandlerState::release_bus;
    } else {
      state_ = HandlerState::active_receive_slave;
    }
  } else if (byte == Symbols::nak &&
             !active_master_repeated_) {  // Negative ACK, retry master
                                          // message
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
    callOnError(LogLevel::error, "errorActiveMasterACK",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();  // Reset active state
    callWrite(Symbols::syn);
    state_ = HandlerState::release_bus;
  }
}

void Handler::activeReceiveSlave(uint8_t byte) {
  active_slave_.pushBack(byte);

  if (active_slave_.size() == 1) active_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == Symbols::ext) active_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (active_slave_.size() >=
      1 + active_slave_dbx_ + 1) {  // 1 byte NN + data + CRC
    active_telegram_.createSlave(active_slave_);
    if (active_telegram_.getSlaveState() == SequenceState::seq_ok) {
      callWrite(Symbols::ack);
      state_ = HandlerState::active_send_slave_positive_acknowledge;
    } else {
      if (monitor_)
        monitor_->updateHandler([](auto& m) { m.error_active_slave++; });
      callOnError(LogLevel::error, "errorActiveSlave",
                  {active_master_.data(), active_master_.size()},
                  {active_slave_.data(), active_slave_.size()});
      active_slave_.clear();  // Clear slave response
      active_slave_dbx_ = 0;
      callWrite(Symbols::nak);
      state_ = HandlerState::active_send_slave_negative_acknowledge;
    }
  }
}

void Handler::activeSendSlavePositiveAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  callOnTelegram(
      MessageType::active, TelegramType::master_slave,
      {active_telegram_.getMaster().data(),
       active_telegram_.getMaster().size()},
      {active_telegram_.getSlave().data(), active_telegram_.getSlave().size()});
  if (monitor_)
    monitor_->updateHandler([](auto& m) { m.messages_active_master_slave++; });
  callActiveReset();  // Reset active state
  callWrite(Symbols::syn);
  state_ = HandlerState::release_bus;
}

void Handler::activeSendSlaveNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  if (!active_slave_repeated_) {
    active_slave_repeated_ = true;
    state_ = HandlerState::active_receive_slave;
  } else {
    if (monitor_)
      monitor_->updateHandler([](auto& m) { m.error_active_slave_ack++; });
    callOnError(LogLevel::error, "errorActiveSlaveACK",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();  // Reset active state
    callWrite(Symbols::syn);
    state_ = HandlerState::release_bus;
  }
}

void Handler::releaseBus([[maybe_unused]] uint8_t byte) {
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
void Handler::checkPassiveBuffers() {
  if (passive_master_.size() > 0 || passive_slave_.size() > 0) {
    callOnError(LogLevel::info, "checkPassiveBuffers",
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
 * Finally, it resets the active communication state to ensure the system
 * can recover and continue operating.
 */
void Handler::checkActiveBuffers() {
  if (active_master_.size() > 0 || active_slave_.size() > 0) {
    callOnError(LogLevel::info, "checkActiveBuffers",
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});

    if (monitor_) monitor_->updateHandler([](auto& m) { m.reset_active++; });

    callActiveReset();
  }
}

void Handler::callPassiveReset() {
  passive_telegram_.clear();

  passive_master_.clear();
  passive_master_dbx_ = 0;
  passive_master_repeated_ = false;

  passive_slave_.clear();
  passive_slave_dbx_ = 0;
  passive_slave_index_ = 0;
  passive_slave_repeated_ = false;
}

void Handler::callActiveReset() {
  active_message_ = false;
  active_telegram_.clear();

  active_master_.clear();
  active_master_index_ = 0;
  active_master_repeated_ = false;

  active_slave_.clear();
  active_slave_dbx_ = 0;
  active_slave_repeated_ = false;
}

void Handler::callWrite(uint8_t byte) { pending_write_ = byte; }

void Handler::callOnBusRequestWon() {
  if (bus_request_won_callback_) {
    if (monitor_) monitor_->callback_won.markBegin();
    bus_request_won_callback_();
    if (monitor_) monitor_->callback_won.markEnd();
  }
}

void Handler::callOnBusRequestLost() {
  if (bus_request_lost_callback_) {
    if (monitor_) monitor_->callback_lost.markBegin();
    bus_request_lost_callback_();
    if (monitor_) monitor_->callback_lost.markEnd();
  }
}

void Handler::callOnReactiveMasterSlave(ByteView master_view,
                                        Sequence& slave_response) {
  if (reactive_master_slave_callback_) {
    if (monitor_) monitor_->callback_reactive.markBegin();
    reactive_master_slave_callback_({0, master_view, slave_response});
    if (monitor_) monitor_->callback_reactive.markEnd();
  }
}

void Handler::callOnTelegram(MessageType message_type,
                             TelegramType telegram_type, ByteView master_view,
                             ByteView slave_view) {
  if (telegram_callback_) {
    if (monitor_) monitor_->callback_telegram.markBegin();
    telegram_callback_({0, 0, message_type, telegram_type, state_,
                        request_->getState(), master_view, slave_view});
    if (monitor_) monitor_->callback_telegram.markEnd();
  }
}

void Handler::callOnError(LogLevel level, std::string_view error_message,
                          ByteView master_view, ByteView slave_view) {
  if (error_callback_) {
    double current_util = 0.0;
    if (monitor_) {
      monitor_->callback_error.markBegin();
      monitor_->updateBus([](auto& m) {
        m.last_error_timestamp = std::chrono::system_clock::now();
      });
      current_util = monitor_->getMetrics().bus.utilization;
    }
    error_callback_({0, level, error_message, last_result_, state_,
                     request_->getState(), master_view, slave_view,
                     current_util});
    if (monitor_) monitor_->callback_error.markEnd();
  }
}

}  // namespace ebus::detail
