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
#endif

#include "Utils/Common.hpp"

ebus::ClientManager::ClientManager(Bus* bus, BusHandler* busHandler,
                                   Request* request)
    : bus_(bus),
      busHandler_(busHandler),
      request_(request),
      registryDirty_(true),
      busByteQueue_(256),
      running_(false),
      busRequested_(false),
      activeTimeout_(1000) {
  busHandler_->addByteListener(
      [this](const uint8_t& byte) { busByteQueue_.try_push(byte); });

  request_->setExternalBusRequestedCallback([this]() { busRequested_ = true; });
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
    registryDirty_.store(true, std::memory_order_release);
  }
}

void ebus::ClientManager::removeClient(int fd) {
  std::lock_guard<std::mutex> lock(mutex_);
  clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                [fd](const std::shared_ptr<AbstractClient>& c) {
                                  return c->getFd() == fd;
                                }),
                 clients_.end());
  registryDirty_.store(true, std::memory_order_release);
}

void ebus::ClientManager::run() {
  std::shared_ptr<AbstractClient> activeClient = nullptr;
  BusState busState = BusState::Idle;

  std::vector<pollfd> pollFds;
  const int nextTimeout = 10;  // 10ms default timeout for bus responsiveness

  while (running_) {
    bool activity = false;

    // Refresh the client cache only if the registry changed
    if (registryDirty_.load(std::memory_order_acquire)) {
      std::lock_guard<std::mutex> lock(mutex_);
      clientsCache_ = clients_;
      registryDirty_.store(false, std::memory_order_release);

      // Rebuild the poll descriptors for the current session
      pollFds.clear();
      for (auto& client : clientsCache_) {
        pollfd pfd;
        pfd.fd = client->getFd();
        pfd.events = POLLIN;
        pollFds.push_back(pfd);
      }
    }

    {
      // Select new active client if idle
      if (!activeClient && busState == BusState::Idle) {
        // Iterate over the cache without locking!
        for (auto& client : clientsCache_) {
          if (client->isWriteCapable() && client->available()) {
            activeClient = client;
            busState = BusState::Request;
            busRequested_ = false;
            activity = true;
            lastActivityTime_ = std::chrono::steady_clock::now();
            break;
          }
        }
      }
    }

    // Watchdog: Kick active client if it hangs
    if (activeClient && busState != BusState::Idle) {
      auto now = std::chrono::steady_clock::now();
      if (now - lastActivityTime_ > activeTimeout_) {
        activeClient = nullptr;
        busState = BusState::Idle;
        busRequested_ = false;
        request_->reset();
      }
    }

    // Request bus access
    if (activeClient && busState == BusState::Request) {
      if (request_->busAvailable()) {
        uint8_t firstByte = 0;
        if (activeClient->readByte(firstByte)) {
          request_->requestBus(firstByte, true);
          busState = BusState::Response;
          activity = true;
          lastActivityTime_ = std::chrono::steady_clock::now();
        }
      }
    }

    // Transmit to bus once arbitration is won
    if (activeClient && busState == BusState::Transmit) {
      uint8_t sendByte = 0;
      if (activeClient->readByte(sendByte)) {
        bus_->writeByte(sendByte);
        busState = BusState::Response;
        activity = true;
        lastActivityTime_ = std::chrono::steady_clock::now();
      }
    }

    if (processBusBytes(activeClient, busState)) {
      activity = true;
      lastActivityTime_ = std::chrono::steady_clock::now();
    }

    // Efficient wait: Only wait if we haven't done any work this iteration
    // and there is nothing immediately pending in the bus queue.
    if (!activity && busByteQueue_.size() == 0) {
      // We use a small timeout (10ms) to ensure the manager still reacts
      // to bus events even if no client sends data.
      if (!pollFds.empty()) {
#if defined(ESP32)
        // Note: poll() on ESP32 is a wrapper around select() in LWIP
        poll(pollFds.data(), pollFds.size(), nextTimeout);
#elif defined(POSIX)
        poll(pollFds.data(), pollFds.size(), nextTimeout);
#endif
      } else {
        // No clients connected? Just sleep to prevent busy loop
        sleep_ms(nextTimeout);
      }
    }
  }
}

bool ebus::ClientManager::processBusBytes(
    std::shared_ptr<AbstractClient>& activeClient, BusState& busState) {
  uint8_t byte = 0;
  bool processed = false;

  while (busByteQueue_.try_pop(byte)) {
    processed = true;
    bool handledByActive = false;
    std::shared_ptr<AbstractClient> currentActive;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      currentActive = activeClient;
      if (currentActive) {
        if ((busState == BusState::Response ||
             busState == BusState::Transmit) &&
            busRequested_) {
          // Mark as handled to prevent duplicate raw echoes in the broadcast
          // loop below
          handledByActive = true;
          if (currentActive->onBusByte(byte) == Action::Stop) {
            // Client signaled it's done (e.g. error, arbitration loss, or end
            // of telegram)
            activeClient = nullptr;
            busState = BusState::Idle;
            busRequested_ = false;
            request_->reset();
          } else {
            // Arbitration or Payload byte processed: ready for next client
            // transmission.
            if (!request_->busRequestPending()) {
              busState = BusState::Transmit;
            }
          }
        }
      }
    }

    // Perform network I/O outside the lock!
    // This prevents a slow WiFi client from blocking the manager loop.
    for (auto& client : clientsCache_) {
      if (handledByActive && client == currentActive) {
        // currentActive already got the appropriate status or data response.
        continue;
      }
      if (client->isConnected()) {
        client->writeBytes({byte});
      }
    }
  }

  // Clean up disconnected clients outside the byte loop for efficiency
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::remove_if(clients_.begin(), clients_.end(),
                           [](const std::shared_ptr<AbstractClient>& c) {
                             return !c->isConnected();
                           });

  if (it != clients_.end())
    registryDirty_.store(true, std::memory_order_release);

  // If the active client was among those removed, reset the state
  for (auto check = it; check != clients_.end(); ++check) {
    if (*check == activeClient) {
      activeClient = nullptr;
      busState = BusState::Idle;
      busRequested_ = false;
      request_->reset();
    }
  }
  clients_.erase(it, clients_.end());
  return processed;
}