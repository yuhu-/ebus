/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client_manager.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/utils.hpp>
#include <iterator>

#include "core/bus_monitor.hpp"

namespace ebus::detail {

ClientManager::ClientManager(platform::Bus* bus, BusHandler* bus_handler,
                             Request* request, BusMonitor* monitor)
    : bus_(bus),
      bus_handler_(bus_handler),
      request_(request),
      monitor_(monitor),
      running_(false),
      session_state_(SessionState::idle),
      last_state_change_(Clock::now()),
      clients_version_(0),
      last_snapshot_version_(0),
      session_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.session_timeout_ms)),
      transmit_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.transmit_timeout_ms)),
      outbound_buffer_size_(
          ebus::RuntimeConfig{}.network.outbound_buffer_size) {
  if (bus_handler_) {
    bus_listener_id_ = bus_handler_->addByteListener(
        Delegate<void(const BusEventInfo&)>::bind<
            ClientManager, &ClientManager::handleBusEvent>(this));
  }

  if (request_) {
    request_->setExternalBusRequestedCallback(
        Delegate<void()>::bind<ClientManager,
                               &ClientManager::onExternalBusRequested>(this));
  }
}

ClientManager::~ClientManager() {
  stop();
  if (bus_handler_ && bus_listener_id_ != 0) {
    bus_handler_->removeByteListener(bus_listener_id_);
    bus_listener_id_ = 0;
  }
}

void ClientManager::start() {
  if (running_.load(std::memory_order_acquire)) return;
  running_.store(true, std::memory_order_release);
}

void ClientManager::stop() { running_.store(false, std::memory_order_release); }

void ClientManager::setSessionTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  session_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setTransmitTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  transmit_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setOutboundBufferSize(size_t size) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  outbound_buffer_size_ = size;
}

void ClientManager::setBusyPredicate(Delegate<bool()> pred) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  is_busy_ = std::move(pred);
}

bool ClientManager::addClient(int fd, ClientType type) {
  auto client = createClient(fd, request_, type, outbound_buffer_size_);
  if (!client) return false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (clients_.size() >= NetworkLimits::max_clients) {
      client->stop();
      return false;
    }
    clients_.push_back(std::move(client));
    ++clients_version_;
  }
  return true;
}

bool ClientManager::addClient(std::shared_ptr<AbstractClient> client) {
  if (!client) return false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    clients_.push_back(std::move(client));
    ++clients_version_;
  }
  return true;
}

void ClientManager::removeClient(int fd) {
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                       [fd](const std::shared_ptr<AbstractClient>& c) {
                         return c->getFd() == fd;
                       }),
        clients_.end());
    ++clients_version_;
  }
}

bool ClientManager::tick() {
  if (!running_.load(std::memory_order_acquire)) return false;
  bool work_done = false;

  // 1. Snapshot clients only if changed
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients>
      active_clients;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (clients_version_ != last_snapshot_version_) {
      clients_cache_.clear();
      std::copy(clients_.begin(), clients_.end(),
                std::back_inserter(clients_cache_));
      last_snapshot_version_ = clients_version_;
    }
    std::copy(clients_cache_.begin(), clients_cache_.end(),
              std::back_inserter(active_clients));
  }

  // 2. Housekeeping & select active sender
  {
    platform::UniqueLock<platform::Mutex> lock(mutex_);
    if (current_active_sender_ && !current_active_sender_->isConnected()) {
      auto old = current_active_sender_;
      current_active_sender_.reset();
      session_state_ = SessionState::idle;
      bus_requested_.store(false, std::memory_order_release);
      last_error_message_ = "Client disconnected.";
      lock.unlock();
      old->stop();
      request_->reset();
      lock.lock();
      work_done = true;
    }

    if (!current_active_sender_ && session_state_ == SessionState::idle &&
        (!is_busy_ || !is_busy_())) {
      for (auto& client : active_clients) {
        if (client->isConnected() && client->isWriteCapable() &&
            client->wantsToSend()) {
          current_active_sender_ = client;
          client->onSessionStart(++session_counter_);
          session_state_ = SessionState::request;
          last_state_change_ = Clock::now();
          bus_requested_.store(false, std::memory_order_release);
          work_done = true;
          break;
        }
      }
    }
  }

  // 3. Outbound logic
  std::shared_ptr<AbstractClient> active_sender;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    active_sender = current_active_sender_;
  }

  if (active_sender) {
    auto now = Clock::now();
    auto elapsed = now - last_state_change_;
    auto current_timeout = (session_state_ == SessionState::transmit)
                               ? transmit_timeout_
                               : session_timeout_;
    if (elapsed > current_timeout) {
      stopActiveSession();
      work_done = true;
      if (monitor_)
        monitor_->updateRequest([](auto& m) { m.session_timeouts++; });
      last_error_message_ = "Session timed out.";
    } else if (session_state_ == SessionState::request) {
      if (request_->busAvailable()) {
        uint8_t first_byte = 0;
        if (active_sender->recvFromClient(first_byte)) {
          platform::LockGuard<platform::Mutex> lock(mutex_);
          request_->requestBus(first_byte, true);
          last_sent_byte_ = first_byte;
          session_state_ = SessionState::response;
          last_state_change_ = Clock::now();
          work_done = true;
        } else {
          last_error_message_ = "Client sent no data for bus request.";
          stopActiveSession();
          work_done = true;
        }
      }
    } else if (session_state_ == SessionState::transmit) {
      uint8_t send_byte = 0;
      if (active_sender->recvFromClient(send_byte)) {
        bus_->writeByte(send_byte);
        last_sent_byte_ = send_byte;
        platform::LockGuard<platform::Mutex> lock(mutex_);
        session_state_ = SessionState::response;
        last_state_change_ = Clock::now();
        work_done = true;
      }
    }
  }

  // 4. Periodic flush for all clients
  for (auto& client : active_clients) {
    client->tryFlushOutboundBuffer();
  }
  return work_done;
}

void ClientManager::handleBusEvent(const BusEventInfo& info) {
  platform::UniqueLock<platform::Mutex> lock(mutex_);
  if (!running_.load(std::memory_order_acquire)) return;

  if (current_active_sender_) {
    if (session_state_ != SessionState::idle) {
      auto action = current_active_sender_->onBusByte(info);
      if (action == BridgeAction::stop_session) {
        if (monitor_)
          monitor_->updateRequest([](auto& m) { m.bus_request_blocked++; });
        lock.unlock();
        stopActiveSession();
        lock.lock();
      } else if (action == BridgeAction::bypass_wait) {
        session_state_ = SessionState::transmit;
        last_state_change_ = Clock::now();
        bus_requested_.store(false, std::memory_order_release);
      }
    }
  }

  for (auto& client : clients_cache_) {
    if (client != current_active_sender_ && client->isConnected()) {
      client->sendToClient(ByteView(&info.byte, 1));
    }
  }
}

Clock::time_point ClientManager::nextDueTime() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (!running_.load(std::memory_order_acquire))
    return Clock::time_point::max();

  if (current_active_sender_) {
    auto current_timeout = (session_state_ == SessionState::transmit)
                               ? transmit_timeout_
                               : session_timeout_;
    return last_state_change_ + current_timeout;
  }

  // Starvation Fix: If the system is busy, the ClientManager should not
  // request a wakeup "now", as it would lead to a tight spin loop.
  if (is_busy_ && is_busy_()) {
    return Clock::time_point::max();
  }

  return Clock::now() +
         std::chrono::milliseconds(NetworkLimits::wake_interval_ms);
}

ClientManagerStatus ClientManager::fetchStatus() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  ClientManagerStatus s{current_active_sender_ != nullptr,
                        ebus::toString(session_state_), last_error_message_};

  for (const auto& client : clients_) {
    if (client) {
      s.clients.push_back(client->getClientInfo());
    }
  }
  return s;
}

bool ClientManager::isSessionActive() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return session_state_ != SessionState::idle;
}

void ClientManager::onExternalBusRequested() {
  bus_requested_.store(true, std::memory_order_release);
}

void ClientManager::stopActiveSession() {
  std::shared_ptr<AbstractClient> old_sender;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (!current_active_sender_) return;
    old_sender = current_active_sender_;
    current_active_sender_.reset();
    session_state_ = SessionState::idle;
    bus_requested_.store(false, std::memory_order_release);
    last_error_message_ = "Session stopped by ClientManager.";
  }
  // do work outside lock
  if (old_sender) old_sender->stop();
  if (request_) request_->reset();
}

}  // namespace ebus::detail
