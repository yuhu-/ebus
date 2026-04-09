/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "App/ClientManager.hpp"

#include <algorithm>
#include <iostream>

#if defined(ESP32)
#include <lwip/sockets.h>
#elif defined(POSIX)
#include <poll.h>
#include <sys/socket.h>
#endif

#include "Utils/Common.hpp"

ebus::ClientManager::ClientManager(Bus* bus, BusHandler* busHandler,
                                   Request* request)
    : bus_(bus),
      busHandler_(busHandler),
      request_(request),
      busByteQueue_(256),
      running_(false),
      sessionState_(SessionState::Idle) {
  if (busHandler_)
    busHandler_->addByteListener(
        [this](const BusEventContext& ctx) { busByteQueue_.try_push(ctx); });

  if (request_)
    request_->setExternalBusRequestedCallback(
        [this]() { busRequested_.store(true, std::memory_order_release); });
}

ebus::ClientManager::~ClientManager() { stop(); }

void ebus::ClientManager::start() {
  if (running_) return;
  running_ = true;
  worker_ = std::make_unique<ServiceThread>(
      "ebusClientManager", [this] { run(); }, 4096, 3, 0);
  worker_->start();
}

void ebus::ClientManager::stop() {
  running_ = false;
  if (worker_) worker_->join();
}

void ebus::ClientManager::addClient(int fd, ClientType type) {
  auto client = createClient(fd, request_, type);
  if (client) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(std::move(client));  // unique_ptr converts to shared_ptr
  }
}

void ebus::ClientManager::removeClient(int fd) {
  std::lock_guard<std::mutex> lock(mutex_);
  clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                [fd](const std::shared_ptr<AbstractClient>& c) {
                                  return c->getFd() == fd;
                                }),
                 clients_.end());
}

void ebus::ClientManager::run() {
  BusEventContext ctx;
  auto lastStateChange = std::chrono::steady_clock::now();
  SessionState prevState = sessionState_;

  while (running_) {
    // 1. Blocking Pop: wait up to 1ms for bus activity.
    bool hasBusEvent = busByteQueue_.pop(ctx, 1);

    // 2. Snapshot shared state in a single lock block to reduce contention.
    std::vector<std::shared_ptr<AbstractClient>> activeClients;
    std::shared_ptr<AbstractClient> activeSender;
    SessionState currentState;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      activeClients = clients_;

      // Housekeeping: Clean up disconnected active client
      if (currentActiveSender_ && !currentActiveSender_->isConnected()) {
        stopActiveSessionInternal();
      }

      // Housekeeping: Select new active client if idle
      if (!currentActiveSender_ && sessionState_ == SessionState::Idle) {
        for (auto& client : activeClients) {
          if (client->isConnected() && client->isWriteCapable() &&
              client->wantsToSend()) {
            currentActiveSender_ = client;
            sessionState_ = SessionState::Request;
            busRequested_.store(false, std::memory_order_release);
            lastStateChange =
                std::chrono::steady_clock::now();  // update on change
            break;
          }
        }
      }

      if (sessionState_ != prevState) {
        lastStateChange = std::chrono::steady_clock::now();
        prevState = sessionState_;
      }

      activeSender = currentActiveSender_;
      currentState = sessionState_;
    }

    // 3. Outbound Logic: Request bus access or Transmit
    if (activeSender) {
      auto elapsed = std::chrono::steady_clock::now() - lastStateChange;
      // Arbitration phase (Request/Response) depends on bus activity and
      // hardware timing. Streaming phase (Transmit) depends on the
      // responsiveness of the client socket.
      auto timeout = (currentState == SessionState::Transmit)
                         ? std::chrono::milliseconds(500)
                         : std::chrono::seconds(1);

      if (elapsed > timeout) {
        stopActiveSession();
      } else if (currentState == SessionState::Request) {
        if (request_->busAvailable()) {
          uint8_t firstByte = 0;
          if (activeSender->recvFromClient(firstByte)) {
            request_->requestBus(firstByte, true);
            std::lock_guard<std::mutex> lock(mutex_);
            sessionState_ = SessionState::Response;
            lastStateChange = std::chrono::steady_clock::now();
          } else {
            stopActiveSession();
          }
        }
      } else if (currentState == SessionState::Transmit) {
        uint8_t sendByte = 0;
        if (activeSender->recvFromClient(sendByte)) {
          bus_->writeByte(sendByte);
          std::lock_guard<std::mutex> lock(mutex_);
          sessionState_ = SessionState::Response;
          lastStateChange = std::chrono::steady_clock::now();
        }
      }
    }

    // 4. Inbound Logic: Process the byte from the pop() and any others in queue
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
    while (busByteQueue_.try_pop(ctx)) processByte(ctx);

    // 5. Periodic flush for all clients
    for (auto& client : activeClients) {
      client->tryFlushOutboundBuffer();
    }
  }
}

void ebus::ClientManager::stopActiveSessionInternal() {
  if (currentActiveSender_) {
    currentActiveSender_->stop();
    currentActiveSender_ = nullptr;
    sessionState_ = SessionState::Idle;
    busRequested_.store(false, std::memory_order_release);
    request_->reset();
  }
}

void ebus::ClientManager::stopActiveSession() {
  std::lock_guard<std::mutex> lock(mutex_);
  stopActiveSessionInternal();
}
