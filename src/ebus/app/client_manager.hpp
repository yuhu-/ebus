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

#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/status.hpp>

#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "platform/socket.hpp"

namespace ebus::detail {

class BusHandler;
class Request;
class AbstractClient;

/**
 * ClientManager handles all connected clients and routes data between them and
 * the eBus. It supports ReadOnly, Regular, and Enhanced clients.
 */
class ClientManager {
 public:
  // Lifecycle
  ClientManager(platform::Bus* bus, BusHandler* bus_handler, Request* request,
                BusMonitor* bus_monitor);
  ~ClientManager();
  void start(const RuntimeConfig& config = RuntimeConfig{});
  void stop();

  // Special Members & Operators
  ClientManager(const ClientManager&) = delete;
  ClientManager& operator=(const ClientManager&) = delete;

  // Configuration
  void setSessionTimeout(uint32_t timeout_ms);
  void setTransmitTimeout(uint32_t timeout_ms);
  void setMaxSessionAge(uint32_t age_ms);
  void setStuckWithdrawMs(uint32_t withdraw_ms);
  void setOutgoingBufferSize(size_t size);

  // Working Methods
  bool addClient(int fd, ClientType type);
  bool addClient(std::unique_ptr<platform::Socket> socket, ClientType type);
  bool addClient(std::shared_ptr<AbstractClient> client);
  void removeClient(int fd);

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
  BusMonitor* bus_monitor_;

  // Members for the dedicated IO thread
  std::unique_ptr<platform::ServiceThread> worker_;
  std::atomic<bool> running_{false};

  platform::WakeupSignal wakeup_signal_;

  // Queue for bus events (decouples hot-path bus task from client I/O thread)
  platform::Queue<BusEventInfo> bus_queue_;
  std::atomic<size_t> max_bus_queue_{0};

  SessionState session_state_ = SessionState::idle;
  Clock::time_point last_state_change_;
  // Session birth (absolute lifetime cap anchor; idle timeouts refresh on
  // traffic and cannot bound a retry-fed zombie session).
  Clock::time_point session_start_{};
  mutable platform::Mutex mutex_;

  // Internal IO events to replace poll.h macros
  enum IoEvent : uint16_t { io_in = 0x01, io_out = 0x02, io_err = 0x04 };

  // Fixed-size arrays for each client type
  std::array<std::shared_ptr<AbstractClient>, ClientManagerLimits::max_clients>
      regular_clients_;
  std::array<std::shared_ptr<AbstractClient>, ClientManagerLimits::max_clients>
      readonly_clients_;
  std::array<std::shared_ptr<AbstractClient>, ClientManagerLimits::max_clients>
      enhanced_clients_;

  uint32_t session_counter_ = 0;
  std::shared_ptr<AbstractClient> current_active_sender_ = nullptr;
  uint8_t last_sent_byte_ = 0;

  std::string last_error_message_;

  // Listening sockets (must be unique pointers)
  std::unique_ptr<platform::Socket> listen_socket_regular_{nullptr};
  std::unique_ptr<platform::Socket> listen_socket_readonly_{nullptr};
  std::unique_ptr<platform::Socket> listen_socket_enhanced_{nullptr};

  // Keepalive configuration (set in start())
  uint32_t keepalive_idle_sec_ =
      ebus::RuntimeConfig{}.network.keepalive_idle_sec;
  uint32_t keepalive_interval_sec_ =
      ebus::RuntimeConfig{}.network.keepalive_interval_sec;
  uint32_t keepalive_count_ = ebus::RuntimeConfig{}.network.keepalive_count;

  // Configurable timeout for active session
  std::chrono::milliseconds session_timeout_{
      ebus::RuntimeConfig{}.network.session_timeout_ms};
  std::chrono::milliseconds transmit_timeout_{
      ebus::RuntimeConfig{}.network.transmit_timeout_ms};
  // Absolute session lifetime cap (zombie-session bound; see
  // ClientManagerLimits::max_session_age_ms).
  std::chrono::milliseconds max_session_age_{
      ClientManagerLimits::max_session_age_ms};
  // Armed-but-unfired rescue age (SYN-timer path starving while the armed
  // flag locks out the idle fast-path; see stuck_intent_withdraw_ms).
  std::chrono::milliseconds stuck_withdraw_ms_{
      ClientManagerLimits::stuck_intent_withdraw_ms};
  // Intent arm timestamp (set on every successful requestBus).
  Clock::time_point intent_armed_at_{};
  // Session generation at arm time: completion events that arrive after a
  // stop/start (stale fire) must not consume the new session's byte.
  uint32_t armed_generation_ = 0;
  size_t outbound_buffer_size_ =
      ebus::RuntimeConfig{}.network.outbound_buffer_size;

  using ClientArray = std::array<std::shared_ptr<AbstractClient>,
                                 ClientManagerLimits::max_clients>;
  // Request callback target
  void onBusRequested();

  void onBusEventInfo(const BusEventInfo& info);
  void processBusEventInfo(const BusEventInfo& info);

  // Session management helpers (event-driven in new 3-thread architecture)
  void transitSessionState(const SessionState& state);
  void handleBusAvailableForSession();
  void tryStartSessionForClient(std::shared_ptr<AbstractClient>& client);
  void trySendNextByte(std::shared_ptr<AbstractClient>& client);
  // Ends the active session. close_socket=false keeps TCP open for the
  // next request (lost arbitration, cap expiry: peer alive, failed
  // response already sent); true drops dead/silent peers and on shutdown.
  void stopActiveSession(bool close_socket);
  void checkSessionTimeout();
  void handleActiveSenderDisconnected();

  // True with a connected client of any type or an active sender
  // session. Caller must hold mutex_. One short scan shared by the hot
  // path gate and the loop timeout choice (no per-caller state to audit).
  bool hasConsumersLocked() const;

  // Helper to find client by fd across all client arrays mutex_ MUST be locked
  std::shared_ptr<AbstractClient> findClientByFdLocked(int fd);

  // Helper to find client by fd
  std::shared_ptr<AbstractClient> findClientByFd(int fd);

  // Helper to remove client by fd
  void removeClientByFd(int fd);

  std::shared_ptr<AbstractClient> removeClientByFdLocked(int fd);

  void removeDisconnectedClients();

  void addListenerFds(fd_set& readfds, int& max_fd);

  void addClientFdsToSet(const ClientArray& clients, fd_set& readfds,
                         fd_set& writefds, fd_set& exceptfds, int& max_fd);

  int prepareFileDescriptors(fd_set& readfds, fd_set& writefds,
                             fd_set& exceptfds);

  void drainWakeupSignal(fd_set& readfds);

  void acceptNewConnections(fd_set& readfds);

  void handleClientIO(fd_set& readfds, fd_set& writefds, fd_set& exceptfds);

  void handleClientActivity(
      ClientArray& clients, fd_set& readfds, fd_set& writefds,
      fd_set& exceptfds,
      ebus::StaticVector<std::shared_ptr<AbstractClient>,
                         ClientManagerLimits::max_clients * 3>& to_stop);

  void handleSocketInput(
      int fd, std::shared_ptr<AbstractClient>& client,
      ebus::StaticVector<std::shared_ptr<AbstractClient>,
                         ClientManagerLimits::max_clients * 3>& to_stop);

  void handleSocketOutput(
      int fd, std::shared_ptr<AbstractClient>& client,
      ebus::StaticVector<std::shared_ptr<AbstractClient>,
                         ClientManagerLimits::max_clients * 3>& to_stop);

  void clientIoLoop();
  void signalClientIoThread();
};

}  // namespace ebus::detail
