/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client_manager.hpp"

#include <algorithm>
#include <condition_variable>

#if defined(ESP32)
#include <lwip/sockets.h>
#elif defined(POSIX)
#include <poll.h>
#include <sys/socket.h>
#endif

#include "utils/common.hpp"

ebus::ClientManager::ClientManager(Bus* bus, BusHandler* busHandler,
                                   Request* request)
    : bus_(bus),
      busHandler_(busHandler),
      request_(request),
      busByteQueue_(256),
      running_(false),
      sessionState_(SessionState::Idle),
      clientsVersion_(0),
      lastSnapshotVersion_(0),
      activeTimeout_(std::chrono::milliseconds(1000)) {
  if (busHandler_) {
    busListenerId_ =
        busHandler_->addByteListener([this](const BusEventContext& ctx) {
          busByteQueue_.tryPush(ctx);
          notifyWake();
        });
  }

  if (request_) {
    request_->setExternalBusRequestedCallback([this]() {
      busRequested_.store(true, std::memory_order_release);
      notifyWake();
    });
  }
}

ebus::ClientManager::~ClientManager() {
  stop();
  if (busHandler_ && busListenerId_ != 0) {
    busHandler_->removeByteListener(busListenerId_);
    busListenerId_ = 0;
  }
}

void ebus::ClientManager::start() {
  if (running_.load(std::memory_order_acquire)) return;
  running_.store(true, std::memory_order_release);
  worker_ = std::make_unique<ServiceThread>(
      "ebusClientManager", [this] { run(); }, 4096, 3, 0);
  worker_->start();
  notifyWake();
}

void ebus::ClientManager::stop() {
  running_.store(false, std::memory_order_release);
  notifyWake();
  if (worker_) worker_->join();
}

void ebus::ClientManager::setActiveTimeout(std::chrono::milliseconds timeout) {
  activeTimeout_ = timeout;
}

void ebus::ClientManager::addClient(int fd, ClientType type) {
  auto client = createClient(fd, request_, type);
  if (!client) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(std::move(client));
    ++clientsVersion_;
  }
  notifyWake();
}

void ebus::ClientManager::removeClient(int fd) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                       [fd](const std::shared_ptr<AbstractClient>& c) {
                         return c->getFd() == fd;
                       }),
        clients_.end());
    ++clientsVersion_;
  }
  notifyWake();
}

void ebus::ClientManager::run() {
  BusEventContext ctx;

  auto lastStateChange = std::chrono::steady_clock::now();
  SessionState prevState = sessionState_;

  while (running_.load(std::memory_order_acquire)) {
    // 0. Drain immediate bus events first (non-blocking)
    bool hasBusEvent = busByteQueue_.tryPop(ctx);

    // 1. Snapshot clients only if changed
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (clientsVersion_ != lastSnapshotVersion_) {
        clientsCache_ = clients_;  // copy shared_ptrs
        lastSnapshotVersion_ = clientsVersion_;
      }
    }
    auto activeClients = clientsCache_;  // local copy for iteration

    // 2. Housekeeping & select active sender (minimal lock)
    {
      std::unique_lock<std::mutex> lock(mutex_);
      // cleanup disconnected active sender (move out to avoid blocking under
      // lock)
      if (currentActiveSender_ && !currentActiveSender_->isConnected()) {
        // move pointer out, actual stop/reset will happen outside lock
        auto old = currentActiveSender_;
        currentActiveSender_.reset();
        sessionState_ = SessionState::Idle;
        busRequested_.store(false, std::memory_order_release);
        lock.unlock();
        old->stop();
        request_->reset();
        lock.lock();
      }

      // select new active client if idle
      if (!currentActiveSender_ && sessionState_ == SessionState::Idle) {
        for (auto& client : activeClients) {
          if (client->isConnected() && client->isWriteCapable() &&
              client->wantsToSend()) {
            currentActiveSender_ = client;
            sessionState_ = SessionState::Request;
            lastStateChange = std::chrono::steady_clock::now();
            busRequested_.store(false, std::memory_order_release);
            break;
          }
        }
      }

      if (sessionState_ != prevState) {
        prevState = sessionState_;
        lastStateChange = std::chrono::steady_clock::now();
      }
    }

    // 3. Outbound logic: operate on a local copy of active sender to avoid
    // holding lock
    std::shared_ptr<AbstractClient> activeSender;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      activeSender = currentActiveSender_;
    }

    if (activeSender) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = now - lastStateChange;
      auto timeout = (sessionState_ == SessionState::Transmit)
                         ? std::chrono::milliseconds(500)
                         : activeTimeout_;
      if (elapsed > timeout) {
        // stop active session safely
        stopActiveSession();
      } else if (sessionState_ == SessionState::Request) {
        if (request_->busAvailable()) {
          // read first byte from client outside any manager lock
          uint8_t firstByte = 0;
          if (activeSender->recvFromClient(firstByte)) {
            // request the bus (may call into Request but is expected not to
            // re-enter ClientManager)
            {
              std::lock_guard<std::mutex> lock(mutex_);
              request_->requestBus(
                  firstByte, true);  // CRITICAL FIX: Initiate bus arbitration
              sessionState_ = SessionState::Response;
              lastStateChange = std::chrono::steady_clock::now();
            }
          } else {
            stopActiveSession();
          }
        }
      } else if (sessionState_ == SessionState::Transmit) {
        uint8_t sendByte = 0;
        if (activeSender->recvFromClient(sendByte)) {
          bus_->writeByte(sendByte);
          {
            std::lock_guard<std::mutex> lock(mutex_);
            sessionState_ = SessionState::Response;
            lastStateChange = std::chrono::steady_clock::now();
          }
        }
      }
    }

    // 4. Inbound logic: process bus byte(s)
    auto processByte = [&](const BusEventContext& bctx) {
      std::shared_ptr<AbstractClient> currentSender;
      SessionState s;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        currentSender = currentActiveSender_;
        s = sessionState_;
      }

      if (currentSender) {
        if ((s == SessionState::Response || s == SessionState::Transmit) &&
            busRequested_.load(std::memory_order_acquire)) {
          if (currentSender->onBusByte(bctx) == Action::Stop) {
            stopActiveSession();
          } else {
            std::lock_guard<std::mutex> lock(mutex_);
            sessionState_ = SessionState::Transmit;
            lastStateChange = std::chrono::steady_clock::now();
          }
        }
      }

      for (auto& client : activeClients) {
        if (client != currentSender && client->isConnected()) {
          client->sendToClient({bctx.byte});
        }
      }
    };

    if (hasBusEvent) processByte(ctx);
    while (busByteQueue_.tryPop(ctx)) processByte(ctx);

    // 5. Periodic flush for all clients (calls outside lock)
    for (auto& client : activeClients) {
      client->tryFlushOutboundBuffer();
    }

    // 6. If no immediate bus event, wait to reduce busy-wait.
    if (!hasBusEvent) {
      std::unique_lock<std::mutex> lk(wakeMutex_);
      wakeCv_.wait_for(lk, std::chrono::milliseconds(200), [&] {
        return wakeFlag_.load(std::memory_order_acquire) == true ||
               !running_.load(std::memory_order_acquire);
      });
      wakeFlag_.store(false, std::memory_order_release);
    }
  }  // while(running)
}

void ebus::ClientManager::stopActiveSession() {
  std::shared_ptr<AbstractClient> oldSender;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!currentActiveSender_) return;
    oldSender = currentActiveSender_;
    currentActiveSender_.reset();
    sessionState_ = SessionState::Idle;
    busRequested_.store(false, std::memory_order_release);
  }
  // do work outside lock
  if (oldSender) oldSender->stop();
  if (request_) request_->reset();
}

void ebus::ClientManager::notifyWake() {
  {
    std::lock_guard<std::mutex> lk(wakeMutex_);
    wakeFlag_.store(true, std::memory_order_release);
  }
  wakeCv_.notify_one();
}