/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/bus_handler.hpp"

#include <algorithm>

namespace ebus::detail {

BusHandler::BusHandler(Request* request, Handler* handler)
    : request_(request), handler_(handler) {}

BusHandler::~BusHandler() = default;

void BusHandler::setWatchdogTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  watchdog_timeout_ms_ = std::chrono::milliseconds(timeout_ms);
}

void BusHandler::setClientManagerBusEventInfoCallback(
    Delegate<void(const BusEventInfo& info)> callback) {
  client_manager_callback_ = std::move(callback);
}
void BusHandler::setControllerBusEventInfoCallback(
    Delegate<void(const BusEventInfo& info)> callback) {
  controller_callback_ = std::move(callback);
}

void BusHandler::onBusEvent(const BusEvent& bus_event) {
  BusEventInfo info{bus_event.byte,
                    HandlerState::passive_receive_master,
                    RequestState::observe,
                    RequestResult::observe_data,
                    0,
                    bus_event.timestamp};

  if (request_) {
    if (bus_event.bus_request) request_->busRequestCompleted();
    if (bus_event.start_bit) request_->startBit();
    info.request_state = request_->getState();
    info.result = request_->run(bus_event.byte);
    info.lock_counter = request_->getLockCounter();
  }

  if (handler_) {
    handler_->run(info);
    info.handler_state = handler_->getState();
  }

  if (client_manager_callback_) client_manager_callback_(info);

  if (controller_callback_) controller_callback_(info);
}

BusHandlerStatus BusHandler::fetchStatus() const {
  return BusHandlerStatus{
      handler_ ? handler_->getState() : HandlerState::passive_receive_master,
      request_ ? request_->getState() : RequestState::observe};
}

}  // namespace ebus::detail