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
#include <string>
#include <vector>

#include "core/request.hpp"
#include "core/telegram.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "utils/timing_stats.hpp"

namespace ebus {

constexpr uint8_t DEFAULT_ADDRESS = 0xff;

constexpr size_t NUM_HANDLER_STATES = 15;

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

static const char* getHandlerStateText(HandlerState state) {
  const char* values[] = {"passive_receive_master",
                          "passive_receive_master_acknowledge",
                          "passive_receive_slave",
                          "passive_receive_slave_acknowledge",
                          "reactive_send_master_positive_acknowledge",
                          "reactive_send_master_negative_acknowledge",
                          "reactive_send_slave",
                          "reactive_receive_slave_acknowledge",
                          "request_bus",
                          "active_send_master",
                          "active_receive_master_acknowledge",
                          "active_receive_slave",
                          "active_send_slave_positive_acknowledge",
                          "active_send_slave_negative_acknowledge",
                          "release_bus"};
  return values[static_cast<int>(state)];
}

using BusRequestWonCallback = std::function<void()>;
using BusRequestLostCallback = std::function<void()>;

using ReactiveMasterSlaveCallback =
    std::function<void(ByteView master, Sequence& response)>;

#define EBUS_HANDLER_COUNTER_LIST    \
  X(messages_passive_master_slave)   \
  X(messages_passive_master_master)  \
  X(messages_passive_broadcast)      \
  X(messages_active_master_slave)    \
  X(messages_active_master_master)   \
  X(messages_active_broadcast)       \
  X(messages_reactive_master_slave)  \
  X(messages_reactive_master_master) \
  X(reset_passive_00)                \
  X(reset_passive_0704)              \
  X(reset_passive)                   \
  X(reset_active_00)                 \
  X(reset_active_0704)               \
  X(reset_active)                    \
  X(error_passive_master)            \
  X(error_passive_master_ack)        \
  X(error_passive_slave)             \
  X(error_passive_slave_ack)         \
  X(error_reactive_master)           \
  X(error_reactive_master_ack)       \
  X(error_reactive_slave)            \
  X(error_reactive_slave_ack)        \
  X(error_active_master)             \
  X(error_active_master_ack)         \
  X(error_active_slave)              \
  X(error_active_slave_ack)

#define EBUS_HANDLER_TIMING_LIST \
  X(sync)                        \
  X(write)                       \
  X(passive_first)               \
  X(passive_data)                \
  X(active_first)                \
  X(active_data)                 \
  X(callback_won)                \
  X(callback_lost)               \
  X(callback_reactive)           \
  X(callback_telegram)           \
  X(callback_error)

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
  Handler(uint8_t source_address, Bus* bus, Request* request);

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

  void resetMetrics();
  std::map<std::string, MetricValues> getMetrics() const;

 private:
  Bus* bus_ = nullptr;
  Request* request_ = nullptr;
  RequestResult last_result_ = RequestResult::observe_syn;

  uint8_t source_address_ = 0;
  uint8_t target_address_ = 0;

  BusRequestWonCallback bus_request_won_callback_ = nullptr;
  BusRequestLostCallback bus_request_lost_callback_ = nullptr;
  ReactiveMasterSlaveCallback reactive_master_slave_callback_ = nullptr;
  TelegramCallback telegram_callback_ = nullptr;
  ErrorCallback error_callback_ = nullptr;

  std::array<void (Handler::*)(uint8_t), NUM_HANDLER_STATES> state_handlers_ =
      {};

  HandlerState state_ = HandlerState::passive_receive_master;
  HandlerState last_state_ = HandlerState::passive_receive_master;

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_HANDLER_COUNTER_LIST
#undef X
  };

  Counter counter_;

  TimingStats sync_;
  TimingStats write_;
  TimingStats passive_first_;
  TimingStats passive_data_;
  TimingStats active_first_;
  TimingStats active_data_;
  TimingStats callback_won_;
  TimingStats callback_lost_;
  TimingStats callback_reactive_;
  TimingStats callback_telegram_;
  TimingStats callback_error_;

  std::array<TimingStats, NUM_HANDLER_STATES> handler_timing_ = {};

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

  void checkPassiveBuffers();
  void checkActiveBuffers();

  void callPassiveReset();
  void callActiveReset();

  void callWrite(uint8_t byte);

  void callOnBusRequestWon();
  void callOnBusRequestLost();

  void callOnReactiveMasterSlave(ByteView master, Sequence& slave_response);

  void callOnTelegram(MessageType message_type, TelegramType telegram_type,
                      ByteView master_view, ByteView slave_view);

  void callOnError(std::string_view error_message, ByteView master_view,
                   ByteView slave_view);
};

}  // namespace ebus
