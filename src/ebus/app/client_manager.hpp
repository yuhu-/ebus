/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#if !defined(ESP_PLATFORM)
#include <sys/select.h>
#endif

#include <ebus/config.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/status.hpp>

#include "app/bus_access_permit.hpp"
#include "app/client.hpp"
#include "core/bus_handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "platform/socket.hpp"

namespace ebus::detail {

class BusMonitor;

/**
 * ClientManager handles all connected clients and routes data between them and
 * the eBus. It supports ReadOnly, Regular, and Enhanced clients.
 */
class ClientManager {
 public:
  // Internal IO events to replace poll.h macros
  enum IoEvent : uint16_t { io_in = 0x01, io_out = 0x02, io_err = 0x04 };

 public:
  // Lifecycle
  ClientManager(platform::Bus* bus, BusHandler* bus_handler, Request* request,
                BusMonitor* monitor,
                platform::Queue<OrchestrationEvent>* reactor_queue,
                BusAccessPermit* permit = nullptr);
  ~ClientManager();
  void start(const RuntimeConfig& config = RuntimeConfig{});
  void stop();

  // Special Members & Operators
  ClientManager(const ClientManager&) = delete;
  ClientManager& operator=(const ClientManager&) = delete;

  // Configuration
  void setSessionTimeout(uint32_t timeout_ms);
  void setTransmitTimeout(uint32_t timeout_ms);
  void setOutboundBufferSize(size_t size);

  // Sets a predicate to check if the system is too busy to start new bridge
  // sessions.
  void setBusyPredicate(Delegate<bool()> pred);

  // Working Methods
  bool addClient(int fd, ClientType type);
  bool addClient(std::unique_ptr<platform::Socket> socket, ClientType type);
  bool addClient(std::shared_ptr<AbstractClient> client);
  void removeClient(int fd);

  bool tick();
  void processClientIoEvent(int client_fd, uint16_t events);

  // Status/Telemetry
  platform::ServiceThread::Status getThreadStatus() const;
  ClientManagerStatus fetchStatus() const;

  /**
   * @brief Returns true if a bridge session is currently in progress.
   */
  bool isSessionActive() const;

 private:
  platform::Bus* bus_;
  BusHandler* bus_handler_;
  Request* request_;
  BusMonitor* monitor_;
  platform::Queue<OrchestrationEvent>* reactor_queue_;
  BusAccessPermit* permit_ = nullptr;

  std::atomic<bool> running_{false};
  Delegate<bool()> is_busy_;

  SessionState session_state_ = SessionState::idle;
  Clock::time_point last_state_change_;
  mutable platform::Mutex mutex_;

  // Fixed-size arrays for each client type
  std::array<std::shared_ptr<AbstractClient>, NetworkLimits::max_clients>
      regular_clients_;
  std::array<std::shared_ptr<AbstractClient>, NetworkLimits::max_clients>
      readonly_clients_;
  std::array<std::shared_ptr<AbstractClient>, NetworkLimits::max_clients>
      enhanced_clients_;

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

  // Request callback target
  void onExternalBusRequested();

  void onBusEvent(const BusEventInfo& info);

  void stopActiveSession();

  // Helper to find client by fd across all client arrays
  // mutex_ MUST be locked
  std::shared_ptr<AbstractClient> findClientByFdLocked(int fd);

  // Helper to find client by fd
  std::shared_ptr<AbstractClient> findClientByFd(int fd);

  // Helper to remove client by fd
  void removeClientByFd(int fd);

  void removeDisconnectedClients();

  // Members for the dedicated IO thread
  std::unique_ptr<platform::ServiceThread> client_io_worker_;
  std::atomic<bool> client_io_running_{false};
  platform::WakeupSignal wakeup_signal_;

  std::unique_ptr<platform::Socket> listen_socket_regular_;
  std::unique_ptr<platform::Socket> listen_socket_readonly_;
  std::unique_ptr<platform::Socket> listen_socket_enhanced_;

  void clientIoLoop();
  void signalClientIoThread();  // Helper to trigger wakeup_signal_
};

}  // namespace ebus::detail
