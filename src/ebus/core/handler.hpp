/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <chrono>
#include <ebus/callbacks.hpp>
#include <ebus/metrics.hpp>
#include <ebus/types.hpp>
#include <functional>
#include <optional>
#include <vector>

#include "core/telegram.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"

namespace ebus::detail {

class BusMonitor;
class Request;

using BusRequestWonCallback = std::function<void()>;
using BusRequestLostCallback = std::function<void()>;

/**
 * Handler class that implements the eBUS protocol logic as a finite state
 * machine. It processes incoming bytes from the bus, manages state transitions,
 * and invokes callbacks for telegrams and errors. It also collects various
 * metrics about the bus activity. The handler maintains separate buffers and
 * state for passive (observing) and active (transmitting) telegrams, and can
 * react to bus events by sending messages or requesting the bus.
 */
class Handler {
 public:
  Handler(uint8_t source_address, platform::Bus* bus, Request* request,
          BusMonitor* monitor);
  void reset();

  // Configuration
  void setSourceAddress(uint8_t source_address);
  uint8_t getSourceAddress() const;
  uint8_t getTargetAddress() const;
  void setBusRequestWonCallback(BusRequestWonCallback callback);
  void setBusRequestLostCallback(BusRequestLostCallback callback);
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setProtocolCallback(ProtocolCallback callback);

  // Working Methods
  bool sendActiveMessage(ByteView message);
  void run(const BusEventInfo& info);

  // Status/Telemetry
  HandlerState getState() const;
  ebus::SequenceState getActiveSequenceState() const;
  bool isActiveMessagePending() const;
  BusMonitor* getMonitor() const;

 private:
  platform::Bus* bus_ = nullptr;
  Request* request_ = nullptr;
  BusMonitor* monitor_ = nullptr;
  RequestResult last_result_ = RequestResult::observe_syn;

  std::optional<uint8_t> pending_write_;

  uint8_t source_address_ = 0;
  uint8_t target_address_ = 0;

  std::function<void()> bus_request_won_callback_ = nullptr;
  std::function<void()> bus_request_lost_callback_ = nullptr;
  ReactiveMasterSlaveCallback reactive_master_slave_callback_ = nullptr;
  ProtocolCallback protocol_callback_ = nullptr;

  Clock::time_point last_point_;
  bool measure_sync_ = false;

  // passive
  Telegram passive_telegram_;

  Sequence passive_master_;
  size_t passive_master_dbx_ = 0;
  bool passive_master_repeated_ = false;

  Sequence passive_slave_;
  size_t passive_slave_dbx_ = 0;
  size_t passive_slave_index_ = 0;
  bool passive_slave_repeated_ = false;

  // active
  bool active_message_ = false;
  Telegram active_telegram_;

  Sequence active_master_;
  size_t active_master_index_ = 0;
  bool active_master_repeated_ = false;

  Sequence active_slave_;
  size_t active_slave_dbx_ = 0;
  bool active_slave_repeated_ = false;

  void passiveReceiveMaster(uint8_t byte);
  void passiveReceiveMasterAcknowledge(uint8_t byte);
  void passiveReceiveSlave(uint8_t byte);
  void passiveReceiveSlaveAcknowledge(uint8_t byte);
  void reactiveSendMasterPositiveAcknowledge(uint8_t byte);
  void reactiveSendMasterNegativeAcknowledge(uint8_t byte);
  void reactiveSendSlave(uint8_t byte);
  void reactiveReceiveSlaveAcknowledge(uint8_t byte);
  void requestBus(uint8_t byte);
  void activeSendMaster(uint8_t byte);
  void activeReceiveMasterAcknowledge(uint8_t byte);
  void activeReceiveSlave(uint8_t byte);
  void activeSendSlavePositiveAcknowledge(uint8_t byte);
  void activeSendSlaveNegativeAcknowledge(uint8_t byte);
  void releaseBus(uint8_t byte);

  using StateHandler = void (Handler::*)(uint8_t);
  static inline constexpr StateHandler kStateHandlers[] = {
      &Handler::passiveReceiveMaster,
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

  static_assert(sizeof(kStateHandlers) / sizeof(kStateHandlers[0]) ==
                    FsmLimits::num_handler_states,
                "kStateHandlers table size does not match NUM_HANDLER_STATES");

  HandlerState state_ = HandlerState::passive_receive_master;

  void transitionTo(HandlerState next);

  void checkPassiveBuffers();
  void checkActiveBuffers();

  void callPassiveReset();
  void callActiveReset();

  void callWrite(uint8_t byte);

  void callOnBusRequestWon();
  void callOnBusRequestLost();

  void callOnReactiveMasterSlave(ByteView master_view,
                                 Sequence& slave_response);

  void callOnTelegram(MessageType message_type, TelegramType telegram_type,
                      ByteView master_view, ByteView slave_view);

  void callOnError(LogLevel level, ProtocolError protocol_error,
                   SequenceState sequence_state, ByteView master_view,
                   ByteView slave_view);
};

}  // namespace ebus::detail
