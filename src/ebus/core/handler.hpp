/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <chrono>
#include <ebus/definitions.hpp>
#include <ebus/metrics.hpp>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "core/telegram.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_ADDRESS = 0xff;

enum class HandlerState {
  passive_receive_master,
  passive_receive_master_acknowledge,
  passive_receive_slave,
  passive_receive_slave_acknowledge,
  reactive_send_master_positive_acknowledge,
  reactive_send_master_negative_acknowledge,
  reactive_send_slave,
  reactive_receive_slave_acknowledge,
  request_bus,
  active_send_master,
  active_receive_master_acknowledge,
  active_receive_slave,
  active_send_slave_positive_acknowledge,
  active_send_slave_negative_acknowledge,
  release_bus
};

constexpr const char* toString(HandlerState state) {
  switch (state) {
    case HandlerState::passive_receive_master:
      return "passive_receive_master";
    case HandlerState::passive_receive_master_acknowledge:
      return "passive_receive_master_acknowledge";
    case HandlerState::passive_receive_slave:
      return "passive_receive_slave";
    case HandlerState::passive_receive_slave_acknowledge:
      return "passive_receive_slave_acknowledge";
    case HandlerState::reactive_send_master_positive_acknowledge:
      return "reactive_send_master_positive_acknowledge";
    case HandlerState::reactive_send_master_negative_acknowledge:
      return "reactive_send_master_negative_acknowledge";
    case HandlerState::reactive_send_slave:
      return "reactive_send_slave";
    case HandlerState::reactive_receive_slave_acknowledge:
      return "reactive_receive_slave_acknowledge";
    case HandlerState::request_bus:
      return "request_bus";
    case HandlerState::active_send_master:
      return "active_send_master";
    case HandlerState::active_receive_master_acknowledge:
      return "active_receive_master_acknowledge";
    case HandlerState::active_receive_slave:
      return "active_receive_slave";
    case HandlerState::active_send_slave_positive_acknowledge:
      return "active_send_slave_positive_acknowledge";
    case HandlerState::active_send_slave_negative_acknowledge:
      return "active_send_slave_negative_acknowledge";
    case HandlerState::release_bus:
      return "release_bus";
    default:
      return "unknown_state";
  }
}

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
  Handler(uint8_t source_address, Bus* bus, Request* request,
          BusMonitor* monitor);

  void setSourceAddress(uint8_t source_address);
  uint8_t getSourceAddress() const;
  uint8_t getTargetAddress() const;

  void setBusRequestWonCallback(BusRequestWonCallback callback);
  void setBusRequestLostCallback(BusRequestLostCallback callback);
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  HandlerState getState() const;

  bool sendActiveMessage(ByteView message);
  bool isActiveMessagePending() const;

  void reset();

  void run(const BusEventContext& ctx);

 private:
  Bus* bus_ = nullptr;
  Request* request_ = nullptr;
  BusMonitor* monitor_ = nullptr;
  RequestResult last_result_ = RequestResult::observe_syn;

  std::optional<uint8_t> pending_write_;

  uint8_t source_address_ = 0;
  uint8_t target_address_ = 0;

  BusRequestWonCallback bus_request_won_callback_ = nullptr;
  BusRequestLostCallback bus_request_lost_callback_ = nullptr;
  ReactiveMasterSlaveCallback reactive_master_slave_callback_ = nullptr;
  TelegramCallback telegram_callback_ = nullptr;
  ErrorCallback error_callback_ = nullptr;

  std::chrono::steady_clock::time_point last_point_;
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
                    NUM_HANDLER_STATES,
                "kStateHandlers table size does not match NUM_HANDLER_STATES");

  HandlerState state_ = HandlerState::passive_receive_master;
  HandlerState last_state_ = HandlerState::passive_receive_master;

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

  void callOnError(std::string_view error_message, ByteView master_view,
                   ByteView slave_view);
};

}  // namespace ebus
