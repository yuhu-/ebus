/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/handler.hpp"

#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>
#include <utility>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "utils/logger.hpp"

namespace ebus::detail {

namespace {
template <typename... Args>
constexpr uint16_t mask(Args... states) {
  if constexpr (sizeof...(Args) == 0) {
    return 0;
  } else {
    return (... | (1 << static_cast<int>(states)));
  }
}

static constexpr HandlerState reset_trans =
    HandlerState::passive_receive_master;
static constexpr HandlerState arb_trans = HandlerState::request_bus;

bool isPassiveReceiveState(HandlerState state) {
  return state >= HandlerState::passive_receive_master &&
         state <= HandlerState::passive_receive_slave_acknowledge;
}

// FSM Transition Matrix (Spec 4 & 6)

static constexpr uint16_t transition_masks[] = {
    // 0: passive_receive_master
    mask(reset_trans, arb_trans,
         HandlerState::passive_receive_master_acknowledge,
         HandlerState::reactive_send_master_positive_acknowledge,
         HandlerState::reactive_send_master_negative_acknowledge,
         HandlerState::release_bus),
    // 1: passive_receive_master_acknowledge
    mask(reset_trans, arb_trans, HandlerState::passive_receive_slave),
    // 2: passive_receive_slave
    mask(reset_trans, arb_trans,
         HandlerState::passive_receive_slave_acknowledge),
    // 3: passive_receive_slave_acknowledge
    mask(reset_trans, arb_trans, HandlerState::passive_receive_slave),
    // 4: reactive_send_master_positive_acknowledge
    mask(reset_trans, arb_trans, HandlerState::reactive_send_slave),
    // 5: reactive_send_master_negative_acknowledge
    mask(reset_trans, arb_trans),
    // 6: reactive_send_slave
    mask(reset_trans, arb_trans,
         HandlerState::reactive_receive_slave_acknowledge),
    // 7: reactive_receive_slave_acknowledge
    mask(reset_trans, arb_trans, HandlerState::reactive_send_slave),
    // 8: request_bus
    mask(reset_trans, arb_trans, HandlerState::active_send_master,
         HandlerState::release_bus),
    // 9: active_send_master
    mask(reset_trans, arb_trans,
         HandlerState::active_receive_master_acknowledge,
         HandlerState::release_bus),
    // 10: active_receive_master_acknowledge
    mask(reset_trans, arb_trans, HandlerState::active_send_master,
         HandlerState::active_receive_slave, HandlerState::release_bus),
    // 11: active_receive_slave
    mask(reset_trans, arb_trans,
         HandlerState::active_send_slave_positive_acknowledge,
         HandlerState::active_send_slave_negative_acknowledge),
    // 12: active_send_slave_positive_acknowledge
    mask(reset_trans, arb_trans, HandlerState::release_bus),
    // 13: active_send_slave_negative_acknowledge
    mask(reset_trans, arb_trans, HandlerState::active_receive_slave,
         HandlerState::release_bus),
    // 14: release_bus
    mask(reset_trans, arb_trans)};
}  // namespace

Handler::Handler(uint8_t source_address, platform::Bus* bus, Request* request,
                 BusMonitor* bus_monitor)
    : bus_(bus), request_(request), bus_monitor_(bus_monitor) {
  setSourceAddress(source_address);

  request_->setHandlerBusRequestedCallback(
      Delegate<void()>::bind<Handler, &Handler::onBusRequested>(this));

  request_->setStartBitCallback(
      Delegate<void()>::bind<Handler, &Handler::onStartBit>(this));

  // Pre-allocate core buffers to avoid heap allocations in the hot path
  passive_master_.reserve(SequenceLimits::default_capacity);
  passive_slave_.reserve(SequenceLimits::default_capacity);
  active_master_.reserve(SequenceLimits::default_capacity);
  active_slave_.reserve(SequenceLimits::default_capacity);

  last_point_ = Clock::now();
}

void Handler::reset() {
  transitionTo(HandlerState::passive_receive_master);
  callActiveReset();
  callPassiveReset();
}

void Handler::setSourceAddress(uint8_t source_address) {
  source_address_ = ebus::isMaster(source_address)
                        ? source_address
                        : ebus::RuntimeConfig{}.address;
  target_address_ = slaveOf(source_address_);
}

uint8_t Handler::getSourceAddress() const { return source_address_; }

uint8_t Handler::getTargetAddress() const { return target_address_; }

void Handler::setBusRequestWonCallback(Delegate<void()> callback) {
  won_callback_ = std::move(callback);
}

void Handler::setBusRequestLostCallback(Delegate<void()> callback) {
  lost_callback_ = std::move(callback);
}

void Handler::setReactiveCallback(
    Delegate<void(const ReactiveInfo& info)> callback) {
  reactive_callback_ = std::move(callback);
}

void Handler::setProtocolCallback(
    Delegate<void(const ProtocolInfo& info)> callback) {
  protocol_callback_ = std::move(callback);
}

bool Handler::sendActiveMessage(ByteView message) {
  if (active_message_) return false;
  if (message.empty()) return false;

  active_telegram_.createMaster(source_address_, message);
  if (active_telegram_.getMasterState() == SequenceState::seq_ok) {
    active_message_ = true;
    // Proactively request the bus if it's currently idle. This ensures that
    // the low-level physical layer (ISR or Simulation reader) sees the
    // intent before the next SYN arrives on the wire.
    // if (request_) request_->requestBus(source_address_);
  } else {
    // Message failed to build locally (not a bus error): report without
    // touching bus error counters; the scheduler maps this to
    // invalid_message (fatal, never retried, never breaker-counted).
    return false;
  }

  return active_message_;
}

void Handler::run(const BusEventInfo& info) {
  last_result_ = info.result;
  // record timing
  if (info.byte != Symbols::syn) {
    if (active_message_) {
      if (measure_sync_ && bus_monitor_)
        bus_monitor_->active_first.markEnd(info.timestamp);
      else if (bus_monitor_)
        bus_monitor_->active_data.markEnd(info.timestamp);
    } else {
      if (measure_sync_ && bus_monitor_)
        bus_monitor_->passive_first.markEnd(info.timestamp);
      else if (bus_monitor_)
        bus_monitor_->passive_data.markEnd(info.timestamp);
    }
    measure_sync_ = false;
  } else {
    if (measure_sync_ && bus_monitor_)
      bus_monitor_->sync.markEnd(info.timestamp);
    measure_sync_ = true;
  }

  last_point_ = info.timestamp;
  if (bus_monitor_) {
    bus_monitor_->updateHandler(
        [](auto& m) { m.total_observed_protocol_bytes++; });
  }

  if (measure_sync_) {
    if (bus_monitor_) {
      bus_monitor_->sync.markBegin(last_point_);
      bus_monitor_->active_first.markBegin(last_point_);
      bus_monitor_->passive_first.markBegin(last_point_);
    }
  } else {
    if (bus_monitor_) {
      bus_monitor_->active_data.markBegin(last_point_);
      bus_monitor_->passive_data.markBegin(last_point_);
    }
  }

  pending_write_.reset();

  size_t idx = static_cast<size_t>(state_);
  rx_tap_[rx_tap_idx_++ % rx_tap_size] = info.byte;
  if (passive_desync_ && isPassiveReceiveState(state_)) {
    // Framing was lost after a persistent passive error: drop bytes until
    // the next SYN instead of reframing mid-stream tail bytes, which can
    // align into valid-looking phantom telegrams (e.g. QQ=07/ZZ=2b with an
    // ACK-ambiguous 0x00 completing an unvalidated slave part).
    handleDesyncByte(info.byte);
  } else if (idx < FsmLimits::num_handler_states && state_handlers[idx]) {
    (this->*state_handlers[idx])(info.byte);
  }

  // Defer actual bus I/O until after the logic step
  if (bus_) {
    if (pending_write_) {
      if (bus_monitor_) bus_monitor_->write.markBegin();
      bus_->writeByte(*pending_write_);
      pending_write_.reset();
      if (bus_monitor_) {
        bus_monitor_->write.markEnd();
        bus_monitor_->updateHandler(
            [](auto& m) { m.total_sent_protocol_bytes++; });
      }
    }
    if (!pending_write_bulk_.empty()) {
      if (bus_monitor_) bus_monitor_->write.markBegin();
      bus_->writeBytes(pending_write_bulk_);
      const size_t bulk_len = pending_write_bulk_.size();
      pending_write_bulk_.clear();
      if (bus_monitor_) {
        bus_monitor_->write.markEnd();
        bus_monitor_->updateHandler([bulk_len](auto& m) {
          m.total_sent_protocol_bytes += static_cast<uint32_t>(bulk_len);
        });
      }
    }
  }
}

ebus::HandlerState Handler::getState() const { return state_; }

ebus::SequenceState Handler::getActiveSequenceState() const {
  return active_telegram_.getMasterState();
}

bool Handler::isActiveMessagePending() const { return active_message_; }

BusMonitor* Handler::getMonitor() const { return bus_monitor_; }

void Handler::passiveReceiveMaster(uint8_t byte) {
  if (byte != Symbols::syn) {
    // Plausibility Filtering: Check header fields before buffering
    const size_t current_len = passive_master_.size();
    if (current_len == 0) {
      // QQ: Must be a master address
      if (!ebus::isMaster(byte)) {
        if (bus_monitor_)
          bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
        callPassiveReset();
        return;
      }
    } else if (current_len == 1) {
      // ZZ: Must be a valid target (Master/Slave/Broad)
      if (!ebus::isTarget(byte)) {
        if (bus_monitor_)
          bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
        callPassiveReset();
        return;
      }
    } else if (current_len == 2 || current_len == 3) {
      // PB/SB: Must not be AA or A9 (Spec 5.4 & 5.5)
      if (byte == Symbols::syn || byte == Symbols::ext) {
        if (bus_monitor_)
          bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
        callPassiveReset();
        return;
      }
    } else if (current_len == 4) {
      // NN: Number of data bytes must be 0-16
      if (byte > SequenceLimits::max_data_bytes) {
        if (bus_monitor_)
          bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
        callPassiveReset();
        return;
      }
    }

    passive_master_.push_back(byte);

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
          if (bus_monitor_)
            bus_monitor_->updateHandler([](auto& m) { m.messages_passive++; });
          callPassiveReset();
        } else if (passive_master_[1] == source_address_) {
          callWrite(Symbols::ack);
          transitionTo(HandlerState::reactive_send_master_positive_acknowledge);
        } else if (passive_master_[1] == target_address_) {
          passive_slave_.clear();
          callOnReactive(passive_telegram_.getMaster(),
                         passive_slave_);  // slave_response is modified here

          passive_telegram_.createSlave(passive_slave_);
          if (passive_telegram_.getSlaveState() == SequenceState::seq_ok) {
            passive_slave_ =
                passive_telegram_.getSlave();  // Copy the slave response
            passive_slave_.push_back(passive_telegram_.getSlaveCRC(), false);
            passive_slave_.extend();
            callWrite(Symbols::ack);
            transitionTo(
                HandlerState::reactive_send_master_positive_acknowledge);
          } else {
            if (bus_monitor_)
              bus_monitor_->updateHandler([](auto& m) { m.error_reactive++; });
            callOnError(LogLevel::error, ProtocolError::error_reactive_slave,
                        passive_telegram_.getSlaveState(),
                        {passive_master_.data(), passive_master_.size()},
                        {passive_slave_.data(), passive_slave_.size()});
            callPassiveReset();
            callWrite(Symbols::syn);
            transitionTo(HandlerState::release_bus);
          }
        } else {
          transitionTo(HandlerState::passive_receive_master_acknowledge);
        }
      } else {
        if (passive_master_[1] == source_address_ ||
            passive_master_[1] == target_address_) {
          if (bus_monitor_)
            bus_monitor_->updateHandler([](auto& m) { m.error_reactive++; });
          callOnError(LogLevel::error, ProtocolError::error_reactive_master,
                      passive_telegram_.getMasterState(),
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
          passive_telegram_.clear();
          passive_master_.clear();
          passive_master_dbx_ = 0;
          callWrite(Symbols::nak);
          transitionTo(HandlerState::reactive_send_master_negative_acknowledge);
        } else if (passive_telegram_.getType() == TelegramType::master_master ||
                   passive_telegram_.getType() == TelegramType::master_slave) {
          transitionTo(HandlerState::passive_receive_master_acknowledge);
        } else {
          if (bus_monitor_)
            bus_monitor_->updateHandler([](auto& m) { m.error_passive++; });
          callOnError(LogLevel::error, ProtocolError::error_passive_master,
                      passive_telegram_.getMasterState(),
                      {passive_master_.data(), passive_master_.size()},
                      {passive_slave_.data(), passive_slave_.size()});
          callPassiveResync();
        }
      }
    }
  } else {  // Received SYN
    checkPassiveBuffers();
    checkActiveBuffers();

    // Initiate request bus
    if (active_message_ && request_) request_->requestBus(source_address_);
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
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.messages_passive++; });
      callPassiveReset();
      transitionTo(HandlerState::passive_receive_master);
    } else {
      transitionTo(HandlerState::passive_receive_slave);
    }
  } else if (byte != Symbols::syn && !passive_master_repeated_) {
    passive_master_repeated_ = true;
    passive_telegram_.clear();
    passive_master_.clear();
    passive_master_dbx_ = 0;
    transitionTo(HandlerState::passive_receive_master);
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_passive++; });
    callOnError(LogLevel::error, ProtocolError::error_passive_master_ack,
                passive_telegram_.getMasterState(),
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveResync();
  }
}

void Handler::passiveReceiveSlave(uint8_t byte) {
  if (passive_slave_.empty()) {
    // Plausibility: Slave NN must be 0-16
    if (byte > SequenceLimits::max_data_bytes) {
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
      callPassiveReset();
      return;
    }
  }

  passive_slave_.push_back(byte);

  if (passive_slave_.size() == 1) passive_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == Symbols::ext) passive_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (passive_slave_.size() >=
      1 + passive_slave_dbx_ + 1) {  // 1 byte NN + data + CRC
    passive_telegram_.createSlave(passive_slave_);
    if (passive_telegram_.getSlaveState() != SequenceState::seq_ok) {
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.error_passive++; });
      callOnError(LogLevel::error, ProtocolError::error_passive_slave,
                  passive_telegram_.getSlaveState(),
                  {passive_master_.data(), passive_master_.size()},
                  {passive_slave_.data(), passive_slave_.size()});
      if (passive_slave_repeated_) {
        // The retried slave response is corrupt as well: framing is lost,
        // skip to the next SYN instead of lingering for an ACK that would
        // complete an unvalidated slave part (phantom telegrams).
        callPassiveResync();
        return;
      }
    }
    transitionTo(HandlerState::passive_receive_slave_acknowledge);
  }
}

void Handler::passiveReceiveSlaveAcknowledge(uint8_t byte) {
  // An ACK only completes the telegram if the slave part validated: a 0x00
  // data byte is indistinguishable from ACK, so delivering an unvalidated
  // slave would mint phantom telegrams out of misaligned tail bytes.
  if (byte == Symbols::ack &&
      passive_telegram_.getSlaveState() == SequenceState::seq_ok) {
    callOnTelegram(MessageType::passive, TelegramType::master_slave,
                   {passive_telegram_.getMaster().data(),
                    passive_telegram_.getMaster().size()},
                   {passive_telegram_.getSlave().data(),
                    passive_telegram_.getSlave().size()});
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.messages_passive++; });
    callPassiveReset();
    transitionTo(HandlerState::passive_receive_master);
  } else if (byte == Symbols::nak && !passive_slave_repeated_) {
    passive_slave_repeated_ = true;
    passive_slave_.clear();
    passive_slave_dbx_ = 0;
    transitionTo(HandlerState::passive_receive_slave);
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_passive++; });
    callOnError(LogLevel::error, ProtocolError::error_passive_slave_ack,
                passive_telegram_.getSlaveState(),
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveResync();
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
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.messages_reactive++; });
    callPassiveReset();
    transitionTo(HandlerState::passive_receive_master);
  } else {
    if (passive_slave_index_ >= passive_slave_.size()) {
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.error_reactive++; });
      callOnError(LogLevel::error, ProtocolError::illegal_fsm_transition,
                  passive_telegram_.getMasterState(),
                  {passive_master_.data(), passive_master_.size()},
                  {passive_slave_.data(), passive_slave_.size()});
      callPassiveReset();
      transitionTo(HandlerState::passive_receive_master);
      return;
    }
    callWrite(passive_slave_[passive_slave_index_]);  // Send next slave byte
    transitionTo(HandlerState::reactive_send_slave);
  }
}

void Handler::reactiveSendMasterNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  transitionTo(HandlerState::passive_receive_master);
  if (!passive_master_repeated_) {
    passive_master_repeated_ = true;  // Allow one retry
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_reactive++; });
    callOnError(LogLevel::error, ProtocolError::error_reactive_master_ack,
                passive_telegram_.getMasterState(),
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
  }
}

void Handler::reactiveSendSlave([[maybe_unused]] uint8_t byte) {
  passive_slave_index_++;
  if (passive_slave_index_ >= passive_slave_.size())  // All slave bytes sent
    transitionTo(HandlerState::reactive_receive_slave_acknowledge);
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
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.messages_reactive++; });
    callPassiveReset();
    transitionTo(HandlerState::passive_receive_master);
  } else if (byte == Symbols::nak &&
             !passive_slave_repeated_) {  // Negative Symbols::ack, retry slave
                                          // response
    passive_slave_repeated_ = true;
    passive_slave_index_ = 0;
    callWrite(passive_slave_[passive_slave_index_]);
    transitionTo(HandlerState::reactive_send_slave);
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_reactive++; });
    callOnError(LogLevel::error, ProtocolError::error_reactive_slave_ack,
                passive_telegram_.getSlaveState(),
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});
    callPassiveReset();
    transitionTo(HandlerState::passive_receive_master);
  }
}

void Handler::requestBus(uint8_t byte) {
  auto won = [&]() {
    // Discard foreign partials accumulated while our request was pending;
    // our echo is consumed by the active states from here on.
    callPassiveReset();
    // Stale-win guard: the QQ fired via ISR on time, but if this echo
    // arrives past the foreign AUTO-SYN horizon the bus already moved on
    // (idle 40ms+ => real SYN on the wire, Spec 9.2). Preloading now would
    // jam live traffic with our strays and die at idx1 on the stale SYN.
    // Release silently as lost instead: no bulk, no error counters, the
    // scheduler learns via the lost event (breaker counts it honestly).
    // Framing after the blackout is unknown, so resync to the next SYN.
    const uint32_t qq_age_us = bus_ ? bus_->qqWriteAgeUs() : 0;
    // Record every win for the qq_win_age distribution (request section),
    // stale or not — the distribution itself is the diagnostic. Clamp the
    // never-wrote sentinel so it cannot poison last/max.
    if (bus_monitor_ && qq_age_us != UINT32_MAX)
      bus_monitor_->qq_win_age.addSample(qq_age_us);
    if (qq_age_us >= qq_stale_threshold_us_) {
      callOnBusRequestLost();
      active_message_ = false;
      active_telegram_.clear();
      active_master_.clear();
      callPassiveResync();
      return;
    }
    active_master_ = active_telegram_.getMaster();
    active_master_.push_back(active_telegram_.getMasterCRC(), false);
    active_master_.extend();
    if (active_master_.size() > 1) {
      callOnBusRequestWon();
      active_master_index_ = 1;
      // Preload the whole remaining master part into the TX FIFO in one
      // step: a single wakeup instead of one per byte. Echoes are still
      // verified per byte in activeSendMaster; our back-to-back edges keep
      // restarting the foreign AUTO-SYN timer, so each echo keeps a fresh
      // 40ms budget (Spec 9.2) with no further thread wakeups required.
      stageMasterBulk();
      transitionTo(HandlerState::active_send_master);
    } else {
      callOnBusRequestLost();
      active_message_ = false;
      active_telegram_.clear();  // Clear active message state
      active_master_.clear();
      callWrite(Symbols::syn);
      transitionTo(HandlerState::release_bus);
    }
  };

  auto lost = [&]() {
    callOnBusRequestLost();
    // Wire-AND of valid master addresses always yields a valid master: a
    // non-master byte means our arbitration entry hit foreign traffic
    // mid-byte, so it must not be framed as QQ.
    callPassiveReset();
    if (!ebus::isMaster(byte)) {
      callPassiveResync();
      return;
    }
    passive_master_.push_back(byte);
    active_message_ = false;
    active_telegram_.clear();  // Clear active message state
    active_master_.clear();
    active_master_preloaded_ = false;
    transitionTo(HandlerState::passive_receive_master);
  };

  auto error = [&]() {
    callOnBusRequestLost();
    active_message_ = false;
    active_telegram_.clear();  // Clear active message state
    active_master_.clear();
    active_master_preloaded_ = false;
    if (last_result_ == RequestResult::observe_data ||
        last_result_ == RequestResult::retry_error) {
      // The bus turned out busy while we requested it: the dropped byte
      // leaves us mid-telegram, so skip to the next SYN.
      callPassiveResync();
      return;
    }
    callPassiveReset();
    transitionTo(HandlerState::passive_receive_master);
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
  if (active_master_index_ >= active_master_.size()) {
    // Stale/duplicate echo after teardown: count it (pairs with the top
    // entry recorded below) instead of hiding it from error_rate.
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
    callOnError(LogLevel::error, ProtocolError::illegal_fsm_transition,
                active_telegram_.getMasterState(),
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();
    transitionTo(HandlerState::release_bus);
    return;
  }
  if (byte != active_master_[active_master_index_]) {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
    EBUS_LOG_DEBUG_F(
        "[echo_mismatch] idx=%u expected=0x%02x received=0x%02x "
        "active_master_size=%u",
        static_cast<unsigned>(active_master_index_),
        static_cast<unsigned>(active_master_[active_master_index_]),
        static_cast<unsigned>(byte),
        static_cast<unsigned>(active_master_.size()));
    // Stash raw RX history into metrics (no printf on the bus thread!).
    // Read via /metrics echo_mismatch_tap; see rx_tap_ reading guide.
    if (bus_monitor_) {
      bus_monitor_->updateHandler([this](auto& m) {
        for (size_t i = 0; i < 16; ++i)
          m.echo_mismatch_tap[i] = rx_tap_[(rx_tap_idx_ + i) % rx_tap_size];
        m.echo_mismatch_count++;
      });
    }
    callOnError(LogLevel::error, ProtocolError::error_active_master_echo,
                active_telegram_.getMasterState(),
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    // Withdraw silently (no SYN): our abort SYN's echo would otherwise
    // arrive mid-next-attempt and re-trigger this same abort (self-
    // sustaining storm). The scheduler learns via the error event.
    // Note: with a preloaded master part, already-queued remainder bytes
    // may still escape onto the wire here. Accepted by design: post-won
    // contention is near-impossible (wire-AND losers withdraw on the same
    // byte, our back-to-back edges suppress AUTO-SYN), and the alternative
    // — a per-byte wakeup lottery against the 40ms SYN-repeat — is what
    // kills every attempt under load. See stageMasterBulk().
    callActiveReset();
    transitionTo(HandlerState::release_bus);
    return;
  }

  active_master_index_++;
  if (active_master_index_ >= active_master_.size()) {
    if (active_telegram_.getType() == TelegramType::broadcast) {
      callOnTelegram(MessageType::active, TelegramType::broadcast,
                     {active_master_.data(), active_master_.size()},
                     {active_slave_.data(), active_slave_.size()});

      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.messages_active++; });
      callActiveReset();  // Reset active state
      callWrite(Symbols::syn);
      transitionTo(HandlerState::release_bus);
    } else {
      transitionTo(HandlerState::active_receive_master_acknowledge);
    }
  } else if (!active_master_preloaded_) {
    // Fallback: normally the whole remainder sits in the TX FIFO from
    // stageMasterBulk(), so matching echoes advance silently. Only write
    // here if preloading was somehow bypassed.
    callWrite(active_master_[active_master_index_]);
  }
}

void Handler::activeReceiveMasterAcknowledge(uint8_t byte) {
  if (byte == Symbols::ack) {
    if (active_telegram_.getType() == TelegramType::master_master) {
      callOnTelegram(MessageType::active, TelegramType::master_master,
                     {active_master_.data(), active_master_.size()},
                     {active_slave_.data(), active_slave_.size()});
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.messages_active++; });
      callActiveReset();  // Reset active state
      callWrite(Symbols::syn);
      transitionTo(HandlerState::release_bus);
    } else {
      transitionTo(HandlerState::active_receive_slave);
    }
  } else if (byte == Symbols::nak &&
             !active_master_repeated_) {  // Negative ACK, retry master
                                          // message
    active_master_repeated_ = true;
    active_master_index_ = 0;
    callWrite(active_master_[active_master_index_]);
    // Re-preload the remainder behind byte 0: order on the wire is
    // unchanged, per-byte echo verification resumes as usual.
    stageMasterBulk();
    transitionTo(HandlerState::active_send_master);
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
    callOnError(LogLevel::error, ProtocolError::error_active_master_ack,
                active_telegram_.getMasterState(),
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();  // Reset active state
    transitionTo(HandlerState::release_bus);
  }
}

void Handler::activeReceiveSlave(uint8_t byte) {
  if (active_slave_.empty()) {
    // Plausibility: Slave NN must be 0-16
    if (byte > SequenceLimits::max_data_bytes) {
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
      // Pairs with the top entry below: rejected slave garbage is an
      // active error (we NAK and consume the retry, or fail after it).
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
      callOnError(LogLevel::error, ProtocolError::error_active_slave,
                  SequenceState::err_data_byte,
                  {active_master_.data(), active_master_.size()}, {});

      active_slave_.clear();
      active_slave_dbx_ = 0;
      callWrite(Symbols::nak);
      transitionTo(HandlerState::active_send_slave_negative_acknowledge);
      return;
    }
  }

  active_slave_.push_back(byte);

  if (active_slave_.size() == 1) active_slave_dbx_ = byte;

  // AA >> A9 + 01 || A9 >> A9 + 00
  if (byte == Symbols::ext) active_slave_dbx_++;

  // size() > NN + DBx + CRC
  if (active_slave_.size() >=
      1 + active_slave_dbx_ + 1) {  // 1 byte NN + data + CRC
    active_telegram_.createSlave(active_slave_);
    if (active_telegram_.getSlaveState() == SequenceState::seq_ok) {
      callWrite(Symbols::ack);
      transitionTo(HandlerState::active_send_slave_positive_acknowledge);
    } else {
      if (bus_monitor_)
        bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
      callOnError(LogLevel::error, ProtocolError::error_active_slave,
                  active_telegram_.getSlaveState(),
                  {active_master_.data(), active_master_.size()},
                  {active_slave_.data(), active_slave_.size()});
      active_slave_.clear();  // Clear slave response
      active_slave_dbx_ = 0;
      callWrite(Symbols::nak);
      transitionTo(HandlerState::active_send_slave_negative_acknowledge);
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
  if (bus_monitor_)
    bus_monitor_->updateHandler([](auto& m) { m.messages_active++; });
  callActiveReset();  // Reset active state
  callWrite(Symbols::syn);
  transitionTo(HandlerState::release_bus);
}

void Handler::activeSendSlaveNegativeAcknowledge(
    [[maybe_unused]] uint8_t byte) {
  if (!active_slave_repeated_) {
    active_slave_repeated_ = true;
    transitionTo(HandlerState::active_receive_slave);
  } else {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.error_active++; });
    callOnError(LogLevel::error, ProtocolError::error_active_slave_ack,
                active_telegram_.getSlaveState(),
                {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});
    callActiveReset();  // Reset active state
    transitionTo(HandlerState::release_bus);
  }
}

void Handler::releaseBus([[maybe_unused]] uint8_t byte) {
  // If we receive data bytes during bus release (before next SYN), it's noise.
  if (byte != Symbols::syn && bus_monitor_)
    bus_monitor_->updateHandler([](auto& m) { m.invalid_bytes++; });
  transitionTo(HandlerState::passive_receive_master);
}

void Handler::transitionTo(HandlerState next) {
  if (next == state_) return;

  const HandlerState old_state = state_;
  const uint16_t next_bit = 1 << static_cast<int>(next);
  const uint16_t valid_mask = transition_masks[static_cast<size_t>(state_)];

  if (!(next_bit & valid_mask)) {
    // Attribute to the phase we come from so top_errors and error_total
    // stay paired (the 840-vs-0 ghost came from here).
    if (bus_monitor_) {
      bus_monitor_->updateHandler([this, old_state](auto& m) {
        switch (getMessageTypeFromState(old_state)) {
          case MessageType::active:
            m.error_active++;
            break;
          case MessageType::reactive:
            m.error_reactive++;
            break;
          default:
            m.error_passive++;
            break;
        }
      });
    }
    callOnError(LogLevel::error, ProtocolError::illegal_fsm_transition,
                SequenceState::seq_ok, {}, {});
    // Emergency recovery: Force reset to ground state
    next = HandlerState::passive_receive_master;
  }

  state_ = next;
  if (bus_monitor_) {
    bus_monitor_->logHandlerTransition(old_state, next);
  }
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
    auto sequence_state = passive_telegram_.getMasterState();
    if (sequence_state != SequenceState::seq_ok) {
      sequence_state = passive_telegram_.getSlaveState();
    }
    callOnError(LogLevel::info, ProtocolError::check_passive_buffers,
                sequence_state,
                {passive_master_.data(), passive_master_.size()},
                {passive_slave_.data(), passive_slave_.size()});

    if (bus_monitor_) bus_monitor_->logPassiveReset();

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
    auto sequence_state = active_telegram_.getMasterState();
    if (sequence_state != SequenceState::seq_ok) {
      sequence_state = active_telegram_.getSlaveState();
    }
    callOnError(LogLevel::info, ProtocolError::check_active_buffers,
                sequence_state, {active_master_.data(), active_master_.size()},
                {active_slave_.data(), active_slave_.size()});

    if (bus_monitor_) bus_monitor_->logActiveReset();

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

  // Any reset on a completed exchange proves alignment again.
  passive_desync_ = false;
  passive_desync_escape_ = false;
}

void Handler::callPassiveResync() {
  callPassiveReset();
  // Framing is untrustworthy until the next SYN: drop everything meanwhile.
  passive_desync_ = true;
  passive_desync_escape_ = false;
  transitionTo(HandlerState::passive_receive_master);
}

void Handler::handleDesyncByte(uint8_t byte) {
  if (passive_desync_escape_) {
    // Swallow the byte following an escape introducer: it decodes to AA/A9
    // but is never a literal SYN on the wire.
    passive_desync_escape_ = false;
    return;
  }
  if (byte == Symbols::ext) {
    passive_desync_escape_ = true;
    return;
  }
  if (byte != Symbols::syn) {
    if (bus_monitor_)
      bus_monitor_->updateHandler([](auto& m) { m.resync_drops++; });
    return;
  }
  // SYN re-establishes framing: resume with normal SYN handling on empty
  // buffers (no-op buffer check plus pending bus request, if any).
  passive_desync_ = false;
  transitionTo(HandlerState::passive_receive_master);
  passiveReceiveMaster(byte);
}

void Handler::callActiveReset() {
  active_message_ = false;
  active_telegram_.clear();

  active_master_.clear();
  active_master_index_ = 0;
  active_master_repeated_ = false;
  active_master_preloaded_ = false;

  active_slave_.clear();
  active_slave_dbx_ = 0;
  active_slave_repeated_ = false;

  // Drop staged-but-unflushed bytes: without a bus they would go stale,
  // and after a reset they belong to a dead telegram.
  pending_write_.reset();
  pending_write_bulk_.clear();
}

void Handler::callWrite(uint8_t byte) { pending_write_ = byte; }

void Handler::stageMasterBulk() {
  // Copies (no heap: Sequence is stack-backed) so a later active_master_
  // clear cannot dangle the staged block before the end-of-step flush.
  pending_write_bulk_.assignSlice(active_master_, 1);
  active_master_preloaded_ = true;
}

void Handler::onBusRequested() {
  if (active_message_ && state_ != HandlerState::request_bus)
    transitionTo(HandlerState::request_bus);
}

void Handler::onStartBit() {
  // A spurious start bit invalidates both active and passive FSM states
  callActiveReset();
  callPassiveReset();
}

void Handler::callOnBusRequestWon() {
  if (won_callback_) won_callback_();
}

void Handler::callOnBusRequestLost() {
  if (lost_callback_) lost_callback_();
}

void Handler::callOnReactive(ByteView master_view, Sequence& slave_response) {
  if (reactive_callback_) reactive_callback_({0, master_view, slave_response});
}

void Handler::callOnTelegram(MessageType message_type,
                             TelegramType telegram_type, ByteView master_view,
                             ByteView slave_view) {
  if (protocol_callback_) {
    if (bus_monitor_) {
      uint32_t data_bytes = 0;
      // PB + SB + Master Data
      if (master_view.size() >= 5) {
        data_bytes += 2;               // PB and SB are considered payload data
        data_bytes += master_view[4];  // NN
      }
      // Slave Data
      if (slave_view.size() >= 1) {
        data_bytes += slave_view[0];  // NN
      }
      bus_monitor_->updateHandler([data_bytes, message_type](auto& m) {
        m.total_observed_data_bytes += data_bytes;
        if (message_type == MessageType::active)
          m.total_sent_data_bytes += data_bytes;
      });

      if (telegram_type != TelegramType::broadcast && master_view.size() >= 2) {
        bus_monitor_->recordHandlerSuccess(master_view[1]);
      }
    }

    ProtocolInfo info;
    info.is_error = false;
    info.handler_state = state_;
    info.request_state = request_->getState();
    info.message_type = message_type;
    info.telegram_type = telegram_type;
    info.master_view = master_view;
    info.slave_view = slave_view;
    protocol_callback_(info);
  }
}

void Handler::callOnError(LogLevel level, ProtocolError protocol_error,
                          SequenceState sequence_state, ByteView master_view,
                          ByteView slave_view) {
  if (protocol_callback_) {
    // Telemetry (bus last-error stamp, top addresses) tracks ERROR-level
    // events only: INFO-level diagnostics (buffer checks) must neither
    // move last_error_us nor pollute top_errors. The callback still
    // forwards everything with its level intact for loggers.
    if (level == LogLevel::error && bus_monitor_) {
      bus_monitor_->recordBusError();
      bus_monitor_->recordHandlerError(master_view.empty() ? 0xff
                                                           : master_view[0]);
    }

    ProtocolInfo info;
    info.is_error = true;
    info.level = level;
    info.handler_state = state_;
    info.request_state = request_->getState();
    info.message_type = getMessageTypeFromState(state_);
    info.protocol_error = protocol_error;
    info.result = last_result_;
    info.sequence_state = sequence_state;
    info.master_view = master_view;
    info.slave_view = slave_view;
    protocol_callback_(info);
  }
}

MessageType Handler::getMessageTypeFromState(HandlerState state) const {
  switch (state) {
    case HandlerState::passive_receive_master:
    case HandlerState::passive_receive_master_acknowledge:
    case HandlerState::passive_receive_slave:
    case HandlerState::passive_receive_slave_acknowledge:
      return MessageType::passive;
    case HandlerState::reactive_send_master_positive_acknowledge:
    case HandlerState::reactive_send_master_negative_acknowledge:
    case HandlerState::reactive_send_slave:
    case HandlerState::reactive_receive_slave_acknowledge:
      return MessageType::reactive;
    case HandlerState::request_bus:
    case HandlerState::active_send_master:
    case HandlerState::active_receive_master_acknowledge:
    case HandlerState::active_receive_slave:
    case HandlerState::active_send_slave_positive_acknowledge:
    case HandlerState::active_send_slave_negative_acknowledge:
    case HandlerState::release_bus:
      return MessageType::active;
    default:
      return MessageType::undefined;
  }
}

}  // namespace ebus::detail
