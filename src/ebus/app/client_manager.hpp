/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ebus/metrics.hpp>
#include <ebus/status.hpp>
#include <memory>
#include <mutex>
#include <vector>

#include "app/client.hpp"
#include "core/bus_handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "utils/static_vector.hpp"

namespace ebus::detail {

class BusMonitor;

/**
 * ClientManager handles all connected clients and routes data between them and
 * the eBus. It supports ReadOnly, Regular, and Enhanced clients.
 */
class ClientManager {
 public:
  // Lifecycle
  ClientManager(platform::Bus* bus, BusHandler* bus_handler, Request* request,
                BusMonitor* monitor);
  ~ClientManager();
  void start();
  void stop();

  // Special Members & Operators
  ClientManager(const ClientManager&) = delete;
  ClientManager& operator=(const ClientManager&) = delete;

  // Configuration
  void setSessionTimeout(uint32_t timeout_ms);
  void setTransmitTimeout(uint32_t timeout_ms);
  void setOutboundBufferSize(size_t size);

  // Working Methods
  void addClient(int fd, ClientType type);
  void addClient(std::shared_ptr<AbstractClient> client);
  void removeClient(int fd);
  bool tick();
  void handleBusEvent(const BusEventInfo& info);

  // Status/Telemetry
  Clock::time_point nextDueTime() const;
  ClientManagerStatus getStatus();

 private:
  platform::Bus* bus_;
  BusHandler* bus_handler_;
  Request* request_;
  BusMonitor* monitor_;

  std::atomic<bool> running_{false};

  SessionState session_state_ = SessionState::idle;
  Clock::time_point last_state_change_;

  mutable std::mutex mutex_;
  StaticVector<std::shared_ptr<AbstractClient>, NetworkLimits::max_clients>
      clients_;
  StaticVector<std::shared_ptr<AbstractClient>, NetworkLimits::max_clients>
      clients_cache_;

  // Versioning to avoid copying clients_ every loop iteration unless changed.
  std::atomic<uint64_t> clients_version_{0};
  uint64_t last_snapshot_version_{0};

  uint32_t session_counter_ = 0;
  std::shared_ptr<AbstractClient> current_active_sender_ = nullptr;
  std::atomic<bool> bus_requested_{false};
  uint8_t last_sent_byte_ = 0;

  std::string last_error_message_;

  // Listener id from BusHandler so we can remove the listener safely.
  uint32_t bus_listener_id_{0};

  // Configurable timeout for active session
  std::chrono::milliseconds session_timeout_{
      ebus::RuntimeConfig{}.network.session_timeout_ms};
  std::chrono::milliseconds transmit_timeout_{
      ebus::RuntimeConfig{}.network.transmit_timeout_ms};

  size_t outbound_buffer_size_ =
      ebus::RuntimeConfig{}.network.outbound_buffer_size;

  void stopActiveSession();
};

}  // namespace ebus::detail
