/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "App/Client.hpp"
#include "Core/BusHandler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "Platform/Queue.hpp"
#include "Platform/ServiceThread.hpp"

namespace ebus {

/**
 * ClientManager handles all connected clients and routes data between them and
 * the eBus. It supports ReadOnly, Regular, and Enhanced clients.
 */
class ClientManager {
 public:
  ClientManager(Bus* bus, BusHandler* busHandler, Request* request);
  ~ClientManager();

  void start();
  void stop();

  void setActiveTimeout(std::chrono::milliseconds timeout);

  void addClient(int fd, ClientType type);
  void removeClient(int fd);

 private:
  Bus* bus_;
  BusHandler* busHandler_;
  Request* request_;

  Queue<BusEventContext> busByteQueue_;
  std::atomic<bool> running_{false};

  enum class SessionState {
    Idle,      // Waiting for a client to have data
    Request,   // Bus request pending, waiting for our slot to send
    Response,  // Waiting for arbitration result from eBUS
    Transmit   // Arbitration won, sending telegram body
  };

  SessionState sessionState_ = SessionState::Idle;

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<AbstractClient>> clients_;
  std::vector<std::shared_ptr<AbstractClient>> clientsCache_;

  // Versioning to avoid copying clients_ every loop iteration unless changed.
  std::atomic<uint64_t> clientsVersion_{0};
  uint64_t lastSnapshotVersion_{0};

  std::unique_ptr<ServiceThread> worker_;

  std::shared_ptr<AbstractClient> currentActiveSender_ = nullptr;
  std::atomic<bool> busRequested_{false};

  // Wake primitives to reduce busy-waiting
  std::mutex wakeMutex_;
  std::condition_variable wakeCv_;
  std::atomic<bool> wakeFlag_{false};

  // Listener id from BusHandler so we can remove the listener safely.
  uint32_t busListenerId_{0};

  // Configurable timeout for active session
  std::chrono::milliseconds activeTimeout_{1000};

  void run();

  void stopActiveSession();

  void notifyWake();
};

}  // namespace ebus
