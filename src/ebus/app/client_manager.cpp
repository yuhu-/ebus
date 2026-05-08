/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client_manager.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"

namespace ebus::detail {

ClientManager::ClientManager(platform::Bus* bus, BusHandler* bus_handler,
                             Request* request, BusMonitor* monitor)
    : bus_(bus),
      bus_handler_(bus_handler),
      request_(request),
      monitor_(monitor),
      bus_byte_queue_(BusLimits::queue_size),
      running_(false),
      session_state_(SessionState::idle),
      clients_version_(0),
      last_snapshot_version_(0),
      session_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.session_timeout_ms)),
      transmit_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.transmit_timeout_ms)),
      outbound_buffer_size_(
          ebus::RuntimeConfig{}.network.outbound_buffer_size) {
  if (bus_handler_) {
    bus_listener_id_ =
        bus_handler_->addByteListener([this](const BusEventContext& ctx) {
          bus_byte_queue_.tryPush(ctx);
          notifyWake();
        });
  }

  if (request_) {
    request_->setExternalBusRequestedCallback([this]() {
      bus_requested_.store(true, std::memory_order_release);
      notifyWake();
    });
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
  worker_ = std::make_unique<platform::ServiceThread>(
      "ebusClientManager", [this] { run(); },
      OrchestrationLimits::stack_size_low, OrchestrationLimits::priority_med);
  worker_->start();
  notifyWake();
}

void ClientManager::stop() {
  running_.store(false, std::memory_order_release);
  notifyWake();
  if (worker_) worker_->join();
}

void ClientManager::setSessionTimeout(uint32_t timeout_ms) {
  session_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setTransmitTimeout(uint32_t timeout_ms) {
  transmit_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setOutboundBufferSize(size_t size) {
  outbound_buffer_size_ = size;
}

void ClientManager::addClient(int fd, ClientType type) {
  auto client = createClient(fd, request_, type, outbound_buffer_size_);
  if (!client) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (clients_.size() >= NetworkLimits::max_clients) {
      client->stop();
      return;
    }
    clients_.push_back(std::move(client));
    ++clients_version_;
  }
  notifyWake();
}

void ClientManager::addClient(std::shared_ptr<AbstractClient> client) {
  if (!client) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(std::move(client));
    ++clients_version_;
  }
  notifyWake();
}

void ClientManager::removeClient(int fd) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                       [fd](const std::shared_ptr<AbstractClient>& c) {
                         return c->getFd() == fd;
                       }),
        clients_.end());
    ++clients_version_;
  }
  notifyWake();
}

void ClientManager::wake() { notifyWake(); }

size_t ClientManager::queueSize() { return bus_byte_queue_.size(); }

size_t ClientManager::queueCapacity() const { return BusLimits::queue_size; }

platform::ServiceThread::Status ClientManager::getThreadStatus() const {
  if (worker_) {
    return worker_->status();
  }
  return platform::ServiceThread::Status{-1, -1};
}

void ClientManager::run() {
  BusEventContext ctx;

  auto last_state_change = std::chrono::steady_clock::now();
  SessionState prev_state = session_state_;

  while (running_.load(std::memory_order_acquire)) {
    // 0. Drain immediate bus events first (non-blocking)
    bool has_bus_event = bus_byte_queue_.tryPop(ctx);
    bool activity = has_bus_event;

    // 1. Snapshot clients only if changed
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (clients_version_ != last_snapshot_version_) {
        clients_cache_ = clients_;  // copy shared_ptrs
        last_snapshot_version_ = clients_version_;
      }
    }
    auto active_clients = clients_cache_;  // local copy for iteration

    // 2. Housekeeping & select active sender (minimal lock)
    {
      std::unique_lock<std::mutex> lock(mutex_);
      // cleanup disconnected active sender (move out to avoid blocking under
      // lock)
      if (current_active_sender_ && !current_active_sender_->isConnected()) {
        // move pointer out, actual stop/reset will happen outside lock
        auto old = current_active_sender_;
        current_active_sender_.reset();
        session_state_ = SessionState::idle;
        bus_requested_.store(false, std::memory_order_release);
        lock.unlock();
        old->stop();
        request_->reset();
        lock.lock();
      }

      // select new active client if idle
      if (!current_active_sender_ && session_state_ == SessionState::idle) {
        for (auto& client : active_clients) {
          if (client->isConnected() && client->isWriteCapable() &&
              client->wantsToSend()) {
            current_active_sender_ = client;
            client->onSessionStart(++session_counter_);
            session_state_ = SessionState::request;
            last_state_change = std::chrono::steady_clock::now();
            bus_requested_.store(false, std::memory_order_release);
            break;
          }
        }
      }

      if (session_state_ != prev_state) {
        prev_state = session_state_;
        last_state_change = std::chrono::steady_clock::now();
      }
    }

    // 3. Outbound logic: operate on a local copy of active sender to avoid
    // holding lock
    std::shared_ptr<AbstractClient> active_sender;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      active_sender = current_active_sender_;
    }

    if (active_sender) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = now - last_state_change;
      auto current_timeout = (session_state_ == SessionState::transmit)
                                 ? transmit_timeout_
                                 : session_timeout_;
      if (elapsed > current_timeout) {
        // stop active session safely
        stopActiveSession();
        if (monitor_)
          monitor_->updateRequest([](auto& m) { m.session_timeouts++; });
      } else if (session_state_ == SessionState::request) {
        if (request_->busAvailable()) {
          // read first byte from client outside any manager lock
          uint8_t first_byte = 0;
          if (active_sender->recvFromClient(first_byte)) {
            // request the bus (may call into Request but is expected not to
            // re-enter ClientManager)
            {
              std::lock_guard<std::mutex> lock(mutex_);
              request_->requestBus(
                  first_byte, true);  // CRITICAL FIX: Initiate bus arbitration
              last_sent_byte_ = first_byte;
              session_state_ = SessionState::response;
              activity = true;
              last_state_change = std::chrono::steady_clock::now();
            }
          } else {
            stopActiveSession();
          }
        }
      } else if (session_state_ == SessionState::transmit) {
        uint8_t send_byte = 0;
        if (active_sender->recvFromClient(send_byte)) {
          bus_->writeByte(send_byte);
          last_sent_byte_ = send_byte;
          {
            std::lock_guard<std::mutex> lock(mutex_);
            session_state_ = SessionState::response;
            activity = true;
            last_state_change = std::chrono::steady_clock::now();
          }
        }
      }
    }

    // 4. Inbound logic: process bus byte(s)
    auto process_byte = [&](const BusEventContext& bctx) {
      std::shared_ptr<AbstractClient> current_sender;
      SessionState s;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        current_sender = current_active_sender_;
        s = session_state_;
      }

      if (current_sender) {
        // We forward all bytes to the current active sender so it can sniff
        // while waiting for or performing arbitration.
        if (s != SessionState::idle) {
          auto action = current_sender->onBusByte(bctx);
          if (action == BridgeAction::stop_session) {
            if (monitor_)
              monitor_->updateRequest([](auto& m) {
                m.bus_request_blocked++;
              });  // Track as failure
            stopActiveSession();
          } else if (action == BridgeAction::bypass_wait) {
            std::lock_guard<std::mutex> lock(mutex_);
            session_state_ = SessionState::transmit;
            last_state_change = std::chrono::steady_clock::now();
            bus_requested_.store(false, std::memory_order_release);
          }
        }
      }

      for (auto& client : active_clients) {
        if (client != current_sender && client->isConnected()) {
          client->sendToClient(ByteView(&bctx.byte, 1));
        }
      }
    };

    if (has_bus_event) {
      process_byte(ctx);
    }
    while (bus_byte_queue_.tryPop(ctx)) {
      activity = true;
      process_byte(ctx);
    }

    // 5. Periodic flush for all clients (calls outside lock)
    for (auto& client : active_clients) {
      client->tryFlushOutboundBuffer();
    }

    // 6. If no immediate bus event, wait to reduce busy-wait.
    if (!activity) {
      std::unique_lock<std::mutex> lk(wake_mutex_);
      wake_cv_.wait_for(
          lk, std::chrono::milliseconds(NetworkLimits::wake_interval_ms), [&] {
            return wake_flag_.load(std::memory_order_acquire) == true ||
                   !running_.load(std::memory_order_acquire);
          });
      wake_flag_.store(false, std::memory_order_release);
    }
  }  // while(running)
}

void ClientManager::stopActiveSession() {
  std::shared_ptr<AbstractClient> old_sender;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_active_sender_) return;
    old_sender = current_active_sender_;
    current_active_sender_.reset();
    session_state_ = SessionState::idle;
    bus_requested_.store(false, std::memory_order_release);
  }
  // do work outside lock
  if (old_sender) old_sender->stop();
  if (request_) request_->reset();
}

void ClientManager::notifyWake() {
  {
    std::lock_guard<std::mutex> lk(wake_mutex_);
    wake_flag_.store(true, std::memory_order_release);
  }
  wake_cv_.notify_one();
}

}  // namespace ebus::detail
