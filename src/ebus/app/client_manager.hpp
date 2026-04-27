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

#include "app/client.hpp"
#include "core/bus_events.hpp"
#include "core/bus_handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

namespace ebus::detail {

class BusMonitor;

/**
 * ClientManager handles all connected clients and routes data between them and
 * the eBus. It supports ReadOnly, Regular, and Enhanced clients.
 */
class ClientManager {
 public:
  ClientManager(platform::Bus* bus, BusHandler* bus_handler, Request* request,
                BusMonitor* monitor);
  ~ClientManager();

  void start();
  void stop();

  void setActiveTimeout(uint32_t timeout_ms);
  void setOutboundBufferSize(size_t size);

  void addClient(int fd, ClientType type);
  void addClient(std::shared_ptr<AbstractClient> client);
  void removeClient(int fd);

  /**
   * Signals the manager loop to wake up and process pending work immediately.
   */
  void wake();

 private:
  platform::Bus* bus_;
  BusHandler* bus_handler_;
  Request* request_;
  BusMonitor* monitor_;

  platform::Queue<BusEventContext> bus_byte_queue_;
  std::atomic<bool> running_{false};

  enum class SessionState {
    idle,      // Waiting for a client to have data
    request,   // Bus request pending, waiting for our slot to send
    response,  // Waiting for arbitration result from eBUS
    transmit   // Arbitration won, sending telegram body
  };

  SessionState session_state_ = SessionState::idle;

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<AbstractClient>> clients_;
  std::vector<std::shared_ptr<AbstractClient>> clients_cache_;

  // Versioning to avoid copying clients_ every loop iteration unless changed.
  std::atomic<uint64_t> clients_version_{0};
  uint64_t last_snapshot_version_{0};

  std::unique_ptr<platform::ServiceThread> worker_;

  std::shared_ptr<AbstractClient> current_active_sender_ = nullptr;
  std::atomic<bool> bus_requested_{false};

  // Wake primitives to reduce busy-waiting
  std::mutex wake_mutex_;
  std::condition_variable wake_cv_;
  std::atomic<bool> wake_flag_{false};

  // Listener id from BusHandler so we can remove the listener safely.
  uint32_t bus_listener_id_{0};

  // Configurable timeout for active session
  std::chrono::milliseconds active_timeout_{
      ebus::RuntimeConfig{}.network.client_timeout_ms};

  size_t outbound_buffer_size_ =
      ebus::RuntimeConfig{}.network.outbound_buffer_size;

  void run();

  void stopActiveSession();

  void notifyWake();
};

}  // namespace ebus::detail
