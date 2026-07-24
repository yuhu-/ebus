/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client_manager.hpp"

#include <unistd.h>

#include <cassert>
#include <cstring>  // for strerror
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/utils.hpp>
#include <iostream>

#include "core/bus_monitor.hpp"
#include "platform/socket.hpp"
#include "platform/system.hpp"
#include "utils/logger.hpp"

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
      session_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.session_timeout_ms)),
      transmit_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.transmit_timeout_ms)),
      outbound_buffer_size_(
          ebus::RuntimeConfig{}.network.outbound_buffer_size) {
  assert(bus_ != nullptr && "Bus pointer cannot be null");
  assert(request_ != nullptr && "Request pointer cannot be null");

  // Initialize client arrays
  regular_clients_.fill(nullptr);
  readonly_clients_.fill(nullptr);
  enhanced_clients_.fill(nullptr);

  if (!wakeup_signal_.init()) {
    assert(false && "Failed to create WakeupSignal for ClientManager");
  }

  if (bus_handler_) {
    bus_handler_->setClientManagerBusEventInfoCallback(
        Delegate<void(const BusEventInfo&)>::bind<
            ClientManager, &ClientManager::onBusEventInfo>(this));
  }

  if (request_) {
    request_->setExternalBusRequestedCallback(
        Delegate<void()>::bind<ClientManager, &ClientManager::onBusRequested>(
            this));
  }
}

ClientManager::~ClientManager() {
  std::cout << "[ClientManager] Destroying ClientManager..." << std::endl;
  stop();
  wakeup_signal_.close();
}

void ClientManager::start(const RuntimeConfig& config) {
  if (running_.load(std::memory_order_acquire)) return;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (config.network.enable_server) {
      listen_socket_regular_ = std::make_unique<platform::Socket>(
          platform::Socket::createListenSocket(config.network.port_regular));
      if (listen_socket_regular_->isValid()) {
        std::cout << "[ClientManager] Listening for regular clients on port "
                  << config.network.port_regular << std::endl;
      } else {
        std::cout << "[ClientManager] ERROR: Failed to listen for regular "
                     "clients on port "
                  << config.network.port_regular << std::endl;
      }

      listen_socket_readonly_ = std::make_unique<platform::Socket>(
          platform::Socket::createListenSocket(config.network.port_readonly));
      if (listen_socket_readonly_->isValid()) {
        std::cout << "[ClientManager] Listening for readonly clients on port "
                  << config.network.port_readonly << std::endl;
      } else {
        std::cout << "[ClientManager] ERROR: Failed to listen for readonly "
                     "clients on port "
                  << config.network.port_readonly << std::endl;
      }

      listen_socket_enhanced_ = std::make_unique<platform::Socket>(
          platform::Socket::createListenSocket(config.network.port_enhanced));
      if (listen_socket_enhanced_->isValid()) {
        std::cout << "[ClientManager] Listening for enhanced clients on port "
                  << config.network.port_enhanced << std::endl;
      } else {
        std::cout << "[ClientManager] ERROR: Failed to listen for enhanced "
                     "clients on port "
                  << config.network.port_enhanced << std::endl;
      }
    }
  }

  client_io_running_.store(true);

  // Start the client I/O thread
  client_io_worker_ = std::make_unique<platform::ServiceThread>(
      "ebus_client_io",
      Delegate<void()>::bind<ClientManager, &ClientManager::clientIoLoop>(
          this));
  client_io_worker_->start();

  running_.store(true, std::memory_order_release);
}

void ClientManager::stop() {
  std::cout << "[ClientManager] Stopping ClientManager..." << std::endl;
  running_.store(false, std::memory_order_release);
  client_io_running_.store(false);
  signalClientIoThread();
  if (client_io_worker_) {
    client_io_worker_->join();
    client_io_worker_.reset();
  }

  stopActiveSession();

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    listen_socket_regular_.reset();
    listen_socket_readonly_.reset();
    listen_socket_enhanced_.reset();
  }
}

void ClientManager::setSessionTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  session_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setTransmitTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  transmit_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setOutgoingBufferSize(size_t size) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  outbound_buffer_size_ = size;
}

bool ClientManager::addClient(int fd, ClientType type) {
  platform::setNonBlocking(fd);
  // Explicitly keep TCP_NODELAY disabled (leave Nagle's algorithm ON) on
  // ESP_PLATFORM
#if defined(POSIX)
  int flag = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#endif
  return addClient(std::make_unique<platform::Socket>(fd), type);
}

bool ClientManager::addClient(std::unique_ptr<platform::Socket> socket,
                              ClientType type) {
  // Thread 2 only (called from acceptNewConnections) — no mutex needed
  auto client =
      createClient(std::move(socket), request_, type, outbound_buffer_size_);
  if (!client) return false;

  int new_fd = client->getFd();

  auto& clients = [&]() -> auto& {
    switch (type) {
      case ClientType::regular:
        return regular_clients_;
      case ClientType::read_only:
        return readonly_clients_;
      case ClientType::enhanced:
        return enhanced_clients_;
      default:
        return regular_clients_;
    }
  }();

  // Evict any stale disconnected entries to free slots
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (clients[i] && !clients[i]->isConnected()) {
      std::cout << "[ClientManager] Evicting stale disconnected client fd="
                << clients[i]->getFd() << " at slot " << i << std::endl;
      clients[i]->stop();
      clients[i].reset();
    }
  }

  // Find first empty slot
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (!clients[i]) {
      std::cout << "[ClientManager] Registered client fd=" << new_fd
                << " type=" << static_cast<int>(type) << " at slot " << i
                << std::endl;
      clients[i] = std::move(client);
      return true;
    }
  }

  std::cout << "[ClientManager] ERROR: No free slot for client fd=" << new_fd
            << " type=" << static_cast<int>(type) << std::endl;
  client->stop();
  return false;
}

bool ClientManager::addClient(std::shared_ptr<AbstractClient> client) {
  if (!client) return false;

  // For mock clients (used in tests), default to regular
  // Production code should use addClient(int fd, ClientType type) instead
  ClientType type = ClientType::regular;

  platform::LockGuard<platform::Mutex> lock(mutex_);

  // Select the appropriate array based on type
  auto& clients = [&]() -> auto& {
    switch (type) {
      case ClientType::regular:
        return regular_clients_;
      case ClientType::read_only:
        return readonly_clients_;
      case ClientType::enhanced:
        return enhanced_clients_;
      default:
        // Should not happen, but default to regular to avoid crash on ESP32
        return regular_clients_;
    }
  }();

  // Proactively clean up any disconnected clients in this array to free slots
  // immediately
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (clients[i] && !clients[i]->isConnected()) {
      std::cout
          << "[ClientManager]  Removing disconnected client during addClient."
          << std::endl;
      clients[i]->stop();
      clients[i].reset();
    }
  }

  // Find first empty slot and add the client directly
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (!clients[i]) {
      clients[i] = std::move(client);
      return true;
    }
  }

  // No empty slot
  std::cout << "[ClientManager] No empty slot available for client."
            << std::endl;
  client->stop();
  return false;
}

void ClientManager::removeClient(int fd) { removeClientByFd(fd); }

platform::ServiceThread::Status ClientManager::getThreadStatus() const {
  if (client_io_worker_) {
    return client_io_worker_->status();
  }
  return platform::ServiceThread::Status{"ebus_client_manager", -1, -1};
}

ClientManagerStatus ClientManager::fetchStatus() const {
  auto map =
      [](const platform::ServiceThread::Status& s) -> ebus::ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };

  ebus::StaticVector<ClientInfo, NetworkLimits::max_clients * 3> snapshot;
  bool active = false;
  std::string session_str;
  std::string last_err;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    active = (current_active_sender_ != nullptr);
    session_str = ebus::toString(session_state_);
    last_err = last_error_message_;

    // Collect all clients
    for (auto& client : regular_clients_)
      if (client) snapshot.push_back(client->getClientInfo());
    for (auto& client : readonly_clients_)
      if (client) snapshot.push_back(client->getClientInfo());
    for (auto& client : enhanced_clients_)
      if (client) snapshot.push_back(client->getClientInfo());
  }

  ClientManagerStatus s{map(getThreadStatus()), active, session_str, last_err};
  for (const auto& info : snapshot) {
    s.clients.push_back(info);
  }
  return s;
}

bool ClientManager::isSessionActive() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return session_state_ != SessionState::idle;
}

void ClientManager::onBusRequested() {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (session_state_ == SessionState::request && current_active_sender_) {
    transitSessionState(SessionState::response);  // Waiting for echo evaluation
  }
}

void ClientManager::onBusEventInfo(const BusEventInfo& info) {
  // Collect all connected clients
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      all_clients;
  std::shared_ptr<AbstractClient> active_sender;
  bool has_active_session = false;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (!running_.load(std::memory_order_acquire)) return;
    active_sender = current_active_sender_;
    has_active_session = (session_state_ != SessionState::idle);

    // Snapshot all connected clients
    for (auto& client : regular_clients_)
      if (client && client->isConnected()) all_clients.push_back(client);
    for (auto& client : readonly_clients_)
      if (client && client->isConnected()) all_clients.push_back(client);
    for (auto& client : enhanced_clients_)
      if (client && client->isConnected()) all_clients.push_back(client);

    // Reset session timeout if we have an active session
    if (has_active_session) {
      last_state_change_ = Clock::now();
    }
  }

  // Handle active sender's onBusByte and session state transitions
  if (active_sender && active_sender->isConnected()) {
    BridgeAction action = BridgeAction::keep_active;
    action = active_sender->onBusByte(info);

    if (action != BridgeAction::keep_active) {
      if (action == BridgeAction::stop_session) {
        if (monitor_)
          monitor_->updateRequest([](auto& m) { m.bus_request_blocked++; });
        stopActiveSession();
      } else if (action == BridgeAction::bypass_wait) {
        // Arbitration won - transition to transmit state and send next byte
        {
          platform::LockGuard<platform::Mutex> lock(mutex_);
          transitSessionState(SessionState::transmit);
        }
        // Send the next byte (this will acquire the mutex internally)
        trySendNextByte(active_sender);
      }
    }

    // Session state machine: handle state transitions based on bus events
    // Check current session state under mutex to avoid race conditions
    {
      platform::UniqueLock<platform::Mutex> lock(mutex_);
      if (session_state_ == SessionState::request) {
        lock.unlock();
        handleBusAvailableForSession();
      }
    }
  }

  // Forward byte to all connected clients (excluding active sender)
  for (auto& client : all_clients) {
    if (client == active_sender) continue;  // Skip active sender
    if (client && client->isConnected()) {
      client->enqueueOutgoingData(ByteView(&info.byte, 1));
    }
  }

  // Signal the I/O thread that data has been written and needs flushing
  signalClientIoThread();
}

void ClientManager::transitSessionState(const SessionState& state) {
  session_state_ = state;
  last_state_change_ = Clock::now();
}

void ClientManager::handleBusAvailableForSession() {
  std::shared_ptr<AbstractClient> active_sender;
  bool is_request_state = false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    active_sender = current_active_sender_;
    is_request_state = (session_state_ == SessionState::request);
  }

  if (!is_request_state || !active_sender || !active_sender->isConnected())
    return;

  if (request_->busAvailable()) {
    uint8_t first_byte = 0;
    if (active_sender->popPendingIncomingData(first_byte)) {
      platform::LockGuard<platform::Mutex> lock(mutex_);
      // Verify session is still in request state
      if (session_state_ != SessionState::request ||
          current_active_sender_ != active_sender) {
        return;  // Session state changed, abort
      }
      // active_sender->armSynFilter();

      request_->requestBus(first_byte, true);
      last_sent_byte_ = first_byte;
      transitSessionState(SessionState::response);
    } else {
      last_error_message_ = "Client sent no data for bus request.";
      std::cout << "[ClientManager] Client fd=" << active_sender->getFd()
                << " sent no data for bus request" << std::endl;
      stopActiveSession();
    }
  }
}

void ClientManager::tryStartSessionForClient(
    std::shared_ptr<AbstractClient>& client) {
  if (!client || !client->isConnected() || !client->isWriteCapable() ||
      !client->hasPendingIncomingData()) {
    return;
  }

  platform::LockGuard<platform::Mutex> lock(mutex_);
  // Can only start if no active session and not busy
  if (current_active_sender_ || session_state_ != SessionState::idle) {
    return;
  }

  current_active_sender_ = client;
  uint32_t sid = ++session_counter_;
  transitSessionState(SessionState::request);
  std::cout << "[ClientManager] Session started for client fd="
            << client->getFd() << " sid=" << sid << std::endl;
  client->onSessionStart(sid);
}

void ClientManager::trySendNextByte(std::shared_ptr<AbstractClient>& client) {
  if (!client || !client->isConnected()) return;

  uint8_t send_byte = 0;
  if (client->popPendingIncomingData(send_byte)) {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    bus_->writeByte(send_byte);
    last_sent_byte_ = send_byte;

    if (send_byte == Symbols::syn) {
      // Session complete after clean SYN, reset state but keep client
      // connected.
      transitSessionState(SessionState::idle);
      current_active_sender_.reset();
      std::cout << "[ClientManager] Session complete for client fd="
                << client->getFd() << std::endl;
    } else {
      transitSessionState(SessionState::response);
    }
  }
}

void ClientManager::stopActiveSession() {
  std::shared_ptr<AbstractClient> old_sender;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (!current_active_sender_) return;
    old_sender = current_active_sender_;
    current_active_sender_.reset();
    session_state_ = SessionState::idle;
    last_error_message_ = "Session stopped by ClientManager.";
    std::cout << "[ClientManager] Stopping session for client fd="
              << old_sender->getFd() << std::endl;
  }
  if (old_sender) old_sender->stop();
  if (request_) request_->reset();
}

void ClientManager::checkSessionTimeout() {
  std::shared_ptr<AbstractClient> active_sender;
  SessionState current_state;
  Clock::time_point last_change;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    active_sender = current_active_sender_;
    current_state = session_state_;
    last_change = last_state_change_;
  }

  if (!active_sender) return;

  auto now = Clock::now();
  auto elapsed = now - last_change;
  auto current_timeout = (current_state == SessionState::transmit)
                             ? transmit_timeout_
                             : session_timeout_;

  // Convert timeout to Clock::duration for comparison
  auto timeout_duration =
      std::chrono::duration_cast<Clock::duration>(current_timeout);

  if (elapsed > timeout_duration) {
    std::cout << "[ClientManager] Session timeout in state "
              << ebus::toString(current_state)
              << " for client fd=" << active_sender->getFd() << std::endl;
    last_error_message_ = "Session timed out.";
    stopActiveSession();
    if (monitor_)
      monitor_->updateRequest([](auto& m) { m.session_timeouts++; });
  }
}

void ClientManager::handleActiveSenderDisconnected() {
  std::shared_ptr<AbstractClient> old_sender;
  {
    platform::UniqueLock<platform::Mutex> lock(mutex_);
    if (current_active_sender_ && !current_active_sender_->isConnected()) {
      int lost_fd = current_active_sender_->getFd();
      old_sender = current_active_sender_;
      current_active_sender_.reset();
      session_state_ = SessionState::idle;
      last_error_message_ = "Client disconnected.";
      std::cout << "[ClientManager] Active sender fd=" << lost_fd
                << " disconnected, aborting session" << std::endl;
      lock.unlock();
      old_sender->stop();
      request_->reset();
    }
  }
}

void ClientManager::removeDisconnectedClients() {
  // Thread 2 only — client arrays not shared, no mutex needed
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      to_stop;

  auto collect = [&](ClientArray& arr, const char* type_name) {
    for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
      if (arr[i] && !arr[i]->isConnected()) {
        std::cout << "[ClientManager] Removing disconnected " << type_name
                  << " client fd=" << arr[i]->getFd() << " slot=" << i
                  << std::endl;
        to_stop.push_back(arr[i]);
        arr[i].reset();
      }
    }
  };

  collect(regular_clients_, "regular");
  collect(readonly_clients_, "readonly");
  collect(enhanced_clients_, "enhanced");

  for (auto& client : to_stop) {
    client->stop();
  }
}

std::shared_ptr<AbstractClient> ClientManager::findClientByFdLocked(int fd) {
  for (auto& client : regular_clients_)
    if (client && client->getFd() == fd) return client;
  for (auto& client : readonly_clients_)
    if (client && client->getFd() == fd) return client;
  for (auto& client : enhanced_clients_)
    if (client && client->getFd() == fd) return client;

  return nullptr;
}

std::shared_ptr<AbstractClient> ClientManager::findClientByFd(int fd) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return findClientByFdLocked(fd);
}

template <typename ClientArrayType>
bool removeFromArrayLocked(const std::shared_ptr<AbstractClient>& client,
                           ClientArrayType& client_array) {
  // mutex_ MUST be locked by caller
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (client_array[i] == client) {
      client_array[i].reset();
      return true;
    }
  }
  return false;
}

void ClientManager::removeClientByFd(int fd) {
  std::shared_ptr<AbstractClient> client;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    client = findClientByFdLocked(fd);
    if (!client) return;

    // Remove from all client arrays (only one will match)
    removeFromArrayLocked(client, regular_clients_);
    removeFromArrayLocked(client, readonly_clients_);
    removeFromArrayLocked(client, enhanced_clients_);
  }  // Lock released before stopping client

  if (client) {
    std::cout << "[ClientManager] Removing client fd=" << fd << std::endl;
    client->stop();
  }
}

std::shared_ptr<AbstractClient> ClientManager::removeClientByFdLocked(int fd) {
  // mutex_ MUST be locked by caller
  auto client = findClientByFdLocked(fd);
  if (!client) return nullptr;

  removeFromArrayLocked(client, regular_clients_);
  removeFromArrayLocked(client, readonly_clients_);
  removeFromArrayLocked(client, enhanced_clients_);

  return client;
}

void ClientManager::addListenerFds(fd_set& readfds, int& max_fd) {
  // Thread 2 only — no mutex needed
  auto add_listener = [&](const std::unique_ptr<platform::Socket>& listener) {
    if (!listener || !listener->isValid()) return;
    int fd = listener->getFd();
    FD_SET(fd, &readfds);
    if (fd > max_fd) max_fd = fd;
  };

  add_listener(listen_socket_regular_);
  add_listener(listen_socket_readonly_);
  add_listener(listen_socket_enhanced_);
}

void ClientManager::addClientFdsToSet(const ClientArray& clients,
                                      fd_set& readfds, fd_set& writefds,
                                      fd_set& exceptfds, int& max_fd) {
  // Thread 2 only — no mutex needed
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    const auto& client = clients[i];
    if (!client || !client->isConnected()) continue;

    int fd = client->getFd();
    FD_SET(fd, &readfds);    // Always monitor for read
    FD_SET(fd, &exceptfds);  // Monitor for errors
    if (client->hasPendingOutgoingData()) {
      FD_SET(fd, &writefds);  // Monitor for write if buffer has data
    }
    if (fd > max_fd) max_fd = fd;
  }
}

int ClientManager::prepareFileDescriptors(fd_set& readfds, fd_set& writefds,
                                          fd_set& exceptfds) {
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  int max_fd = -1;

  // Add wakeup signal file descriptor
  int wakeup_fd = wakeup_signal_.getReadFd();
  if (wakeup_fd >= 0) {
    FD_SET(wakeup_fd, &readfds);
    max_fd = wakeup_fd;
  }

  // Thread 2 only — client arrays and listen sockets are not touched by other
  // threads here, no mutex needed
  addListenerFds(readfds, max_fd);
  addClientFdsToSet(regular_clients_, readfds, writefds, exceptfds, max_fd);
  addClientFdsToSet(readonly_clients_, readfds, writefds, exceptfds, max_fd);
  addClientFdsToSet(enhanced_clients_, readfds, writefds, exceptfds, max_fd);

  return max_fd;
}

void ClientManager::drainWakeupSignal(fd_set& readfds) {
  int wakeup_fd = wakeup_signal_.getReadFd();
  if (wakeup_fd < 0) return;

  if (FD_ISSET(wakeup_fd, &readfds)) {
    wakeup_signal_.drain();
    // std::cout << "[ClientManager] Wakeup signal drained" << std::endl;
  }
}

void ClientManager::acceptNewConnections(fd_set& readfds) {
  // Thread 2 only — listen sockets and client arrays not shared here
  auto try_accept = [&](std::unique_ptr<platform::Socket>& listener,
                        ClientType type) {
    if (!listener || !listener->isValid()) return;

    int listen_fd = listener->getFd();
    if (!FD_ISSET(listen_fd, &readfds)) return;

    int client_fd = listener->accept();
    if (client_fd >= 0) {
      std::cout << "[ClientManager] Connection accepted on listener fd "
                << listen_fd << "-> client fd " << client_fd << " (type "
                << static_cast<int>(type) << ")" << std::endl;
      if (!addClient(client_fd, type)) {
        std::cout << "[ClientManager] Failed to register client fd "
                  << client_fd << std::endl;
        platform::close(client_fd);
      }
    } else {
      std::cout << "[ClientManager] accept() failed on listener fd "
                << listen_fd << ", errno: " << errno << std::endl;
    }
  };

  try_accept(listen_socket_regular_, ClientType::regular);
  try_accept(listen_socket_readonly_, ClientType::read_only);
  try_accept(listen_socket_enhanced_, ClientType::enhanced);
}

void ClientManager::handleClientIO(fd_set& readfds, fd_set& writefds,
                                   fd_set& exceptfds) {
  // Thread 2 only — client arrays not shared, no mutex needed here
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      to_stop;

  handleClientActivity(regular_clients_, readfds, writefds, exceptfds, to_stop);
  handleClientActivity(readonly_clients_, readfds, writefds, exceptfds,
                       to_stop);
  handleClientActivity(enhanced_clients_, readfds, writefds, exceptfds,
                       to_stop);

  for (auto& client : to_stop) {
    client->stop();
  }
}

void ClientManager::handleClientActivity(
    ClientArray& clients, fd_set& readfds, fd_set& writefds, fd_set& exceptfds,
    ebus::StaticVector<std::shared_ptr<AbstractClient>,
                       NetworkLimits::max_clients * 3>& to_stop) {
  // Thread 2 only — no mutex needed for client array iteration
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    auto& client = clients[i];
    if (!client || !client->isConnected()) continue;

    int fd = client->getFd();

    if (FD_ISSET(fd, &exceptfds)) {
      std::cout << "[ClientManager] Exception on client fd=" << fd << std::endl;
      to_stop.push_back(client);
      client.reset();
      continue;
    }

    if (FD_ISSET(fd, &readfds)) {
      handleSocketInput(fd, client, to_stop);
    }

    if (client && FD_ISSET(fd, &writefds)) {
      handleSocketOutput(fd, client, to_stop);
    }
  }
}

void ClientManager::handleSocketInput(
    int fd, std::shared_ptr<AbstractClient>& client,
    ebus::StaticVector<std::shared_ptr<AbstractClient>,
                       NetworkLimits::max_clients * 3>& to_stop) {
  // Thread 2 only — no mutex needed for client array or socket ops
  uint8_t buffer[256];

  while (true) {
    ssize_t bytes_read =
        platform::recv(fd, buffer, sizeof(buffer), platform::Flags::dont_wait);

    if (bytes_read > 0) {
      client->handleIncomingStream(buffer, static_cast<size_t>(bytes_read));

      bool is_active_sender = false;
      bool is_transmit_state = false;
      {
        platform::LockGuard<platform::Mutex> lock(mutex_);
        is_active_sender = (current_active_sender_ == client);
        is_transmit_state = (session_state_ == SessionState::transmit);
      }

      if (is_active_sender && is_transmit_state) {
        trySendNextByte(client);
      } else {
        tryStartSessionForClient(client);
      }
    } else if (bytes_read == 0) {
      std::cout << "[ClientManager] Client fd=" << fd << " closed connection"
                << std::endl;
      to_stop.push_back(client);
      client.reset();
      break;
    } else {
      if (platform::isWouldBlock() || platform::isInterrupted()) {
        break;
      }
      std::cout << "[ClientManager] recv() failed on client fd=" << fd
                << ", errno=" << errno << std::endl;
      to_stop.push_back(client);
      client.reset();
      break;
    }
  }
}

void ClientManager::handleSocketOutput(
    int fd, std::shared_ptr<AbstractClient>& client,
    ebus::StaticVector<std::shared_ptr<AbstractClient>,
                       NetworkLimits::max_clients * 3>& to_stop) {
  (void)fd;
  // Thread 2 only — no mutex needed
  // flushOutgoingData acquires io_mutex_ internally (shared with Thread 1)
  if (!client->flushOutgoingData()) {
    to_stop.push_back(client);
    client.reset();
  }
}

void ClientManager::clientIoLoop() {
  fd_set readfds, writefds, exceptfds;

  while (client_io_running_.load()) {
    // Phase 1: Prepare file descriptor sets for this iteration
    int max_fd = prepareFileDescriptors(readfds, writefds, exceptfds);

    if (max_fd == -1) {
      platform::sleepMilli(NetworkLimits::wake_interval_ms);
      continue;
    }

    // Phase 2: Use a short timeout for responsiveness
    // We check for session timeouts and bus availability in the timeout
    // handler
    struct timeval tv{0, 10000};  // 10ms timeout

    // Phase 3: Block on socket events
    int activity = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);

    if (!client_io_running_.load()) break;

    if (activity < 0) {
      if (errno == EINTR || errno == EBADF) continue;
      std::cerr << "[ClientManager] select() failed: " << strerror(errno)
                << std::endl;
      break;
    }

    // Phase 3.5: On timeout, check for session timeout, disconnected clients,
    // and bus availability for pending sessions
    if (activity == 0) {
      checkSessionTimeout();
      handleActiveSenderDisconnected();
      removeDisconnectedClients();
      // Check if we have a session in request state waiting for bus
      // availability This is a fallback for when no bus events are coming in
      {
        platform::UniqueLock<platform::Mutex> lock(mutex_);
        if (session_state_ == SessionState::request && current_active_sender_) {
          lock.unlock();
          handleBusAvailableForSession();
        }
      }
      continue;
    }

    // Phase 4: Drain wakeup signal if triggered
    drainWakeupSignal(readfds);

    // Phase 5: Accept pending new connections
    acceptNewConnections(readfds);

    // Phase 6: Process client read/write activity
    handleClientIO(readfds, writefds, exceptfds);

    // Phase 7: Cleanup disconnected clients (also on activity)
    removeDisconnectedClients();
  }
}

void ClientManager::signalClientIoThread() { wakeup_signal_.signal(); }

}  // namespace ebus::detail
