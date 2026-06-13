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

uint32_t BusHandler::addByteListener(ByteListener listener) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  uint32_t id = next_listener_id_++;
  listeners_.push_back({id, std::move(listener)});
  listeners_version_++;
  return id;
}

void BusHandler::removeByteListener(uint32_t id) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  listeners_.erase(
      std::remove_if(listeners_.begin(), listeners_.end(),
                     [id](const std::pair<uint32_t, ByteListener>& p) {
                       return p.first == id;
                     }),
      listeners_.end());
  listeners_version_++;
}

void BusHandler::processEvent(const BusEvent& bus_event) {
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

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (listeners_version_ != last_cache_version_) {
      listeners_cache_.clear();
      std::for_each(listeners_.begin(), listeners_.end(),
                    [this](const auto& item) {
                      listeners_cache_.push_back(item.second);
                    });
      last_cache_version_ = listeners_version_;
    }
  }

  if (!listeners_cache_.empty()) {
    for (const auto& listener : listeners_cache_) {
      listener(info);
    }
  }
}

BusHandlerStatus BusHandler::fetchStatus() const {
  return BusHandlerStatus{
      handler_ ? handler_->getState() : HandlerState::passive_receive_master,
      request_ ? request_->getState() : RequestState::observe};
}

}  // namespace ebus::detail