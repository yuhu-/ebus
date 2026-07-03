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

namespace ebus::detail {

ClientManager::ClientManager(
    platform::Bus* bus, BusHandler* bus_handler, Request* request,
    BusMonitor* monitor,
    detail::platform::Queue<OrchestrationEvent>* reactor_queue,
    BusAccessPermit* permit)
    : bus_(bus),
      bus_handler_(bus_handler),
      request_(request),
      monitor_(monitor),
      reactor_queue_(reactor_queue),
      permit_(permit),
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
  assert(reactor_queue_ != nullptr && "Reactor queue pointer cannot be null");

  // Initialize client arrays
  regular_clients_.fill(nullptr);
  readonly_clients_.fill(nullptr);
  enhanced_clients_.fill(nullptr);

  if (!wakeup_signal_.init()) {
    assert(false && "Failed to create WakeupSignal for ClientManager");
  }

  if (bus_handler_) {
    bus_listener_id_ = bus_handler_->addByteListener(
        Delegate<void(const BusEventInfo&)>::bind<ClientManager,
                                                  &ClientManager::onBusEvent>(
            this));
  }

  if (request_) {
    request_->setExternalBusRequestedCallback(
        Delegate<void()>::bind<ClientManager,
                               &ClientManager::onExternalBusRequested>(this));
  }
}

ClientManager::~ClientManager() {
  std::cout << "[ClientManager] Destroying ClientManager..." << std::endl;
  stop();
  if (bus_handler_ && bus_listener_id_ != 0) {
    bus_handler_->removeByteListener(bus_listener_id_);
    bus_listener_id_ = 0;
  }
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
  client_io_worker_ = std::make_unique<platform::ServiceThread>(
      "ebus_client_manager", [this] { clientIoLoop(); },
      OrchestrationLimits::client_manager_stack_size,
      OrchestrationLimits::client_manager_priority);
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

void ClientManager::setOutboundBufferSize(size_t size) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  outbound_buffer_size_ = size;
}

void ClientManager::setBusyPredicate(Delegate<bool()> pred) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  is_busy_ = std::move(pred);
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
  auto client =
      createClient(std::move(socket), request_, type, outbound_buffer_size_);
  if (!client) return false;

  int new_fd = client->getFd();

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
        return regular_clients_;
    }
  }();

  // Proactively clean up stale disconnected entries to free slots
  for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
    if (clients[i] && !clients[i]->isConnected()) {
      std::cout << "[ClientManager] Evicting stale disconnected client fd="
                << clients[i]->getFd() << " at slot " << i << " to make room"
                << std::endl;
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

  // No empty slot
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

bool ClientManager::tick() {
  if (!running_.load(std::memory_order_acquire)) return false;
  bool work_done = false;

  // Remove disconnected clients
  removeDisconnectedClients();

  // Collect all active clients
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      active_clients;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    for (auto& c : regular_clients_)
      if (c && c->isConnected()) active_clients.push_back(c);
    for (auto& c : readonly_clients_)
      if (c && c->isConnected()) active_clients.push_back(c);
    for (auto& c : enhanced_clients_)
      if (c && c->isConnected()) active_clients.push_back(c);
  }

  // Housekeeping: handle disconnected active sender
  {
    platform::UniqueLock<platform::Mutex> lock(mutex_);
    if (current_active_sender_ && !current_active_sender_->isConnected()) {
      int lost_fd = current_active_sender_->getFd();
      auto old = current_active_sender_;
      current_active_sender_.reset();
      session_state_ = SessionState::idle;
      bus_requested_.store(false, std::memory_order_release);
      last_error_message_ = "Client disconnected.";
      std::cout << "[ClientManager] Active sender fd=" << lost_fd
                << " disconnected, aborting session" << std::endl;
      lock.unlock();
      old->stop();
      request_->reset();
      work_done = true;
    }
  }

  // Select new active sender based on pending bus requests
  std::shared_ptr<AbstractClient> selected_client;
  uint32_t selected_sid = 0;
  {
    platform::UniqueLock<platform::Mutex> lock(mutex_);
    if (!current_active_sender_ && session_state_ == SessionState::idle &&
        (!is_busy_ || !is_busy_())) {
      for (auto& client : active_clients) {
        if (client->isConnected() && client->isWriteCapable() &&
            client->hasPendingBusRequest()) {
          if (permit_ && !permit_->tryAcquire(BusAccessPermit::external)) {
            // Bus access denied (arbitration lost) - will retry next tick
            break;
          }
          current_active_sender_ = client;
          selected_sid = ++session_counter_;
          session_state_ = SessionState::request;
          last_state_change_ = Clock::now();
          bus_requested_.store(false, std::memory_order_release);
          work_done = true;
          selected_client = client;
          std::cout << "[ClientManager] Session started for client fd="
                    << client->getFd() << " sid=" << selected_sid << std::endl;
          break;
        }
      }
    }
  }
  if (selected_client) {
    selected_client->onSessionStart(selected_sid);
  }

  // Handle active session state machine
  std::shared_ptr<AbstractClient> active_sender;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    active_sender = current_active_sender_;
  }

  if (active_sender) {
    auto now = Clock::now();
    auto elapsed = now - last_state_change_;
    auto current_timeout = (session_state_ == SessionState::transmit)
                               ? transmit_timeout_
                               : session_timeout_;
    if (elapsed > current_timeout) {
      std::cout << "[ClientManager] Session timeout in state "
                << ebus::toString(session_state_)
                << " for client fd=" << active_sender->getFd() << std::endl;
      last_error_message_ = "Session timed out.";
      stopActiveSession();
      work_done = true;
      if (monitor_)
        monitor_->updateRequest([](auto& m) { m.session_timeouts++; });
    } else if (session_state_ == SessionState::request) {
      if (request_->busAvailable()) {
        uint8_t first_byte = 0;
        if (active_sender->popPendingBusRequest(first_byte)) {
          platform::LockGuard<platform::Mutex> lock(mutex_);
          active_sender->armSynFilter();
          request_->requestBus(first_byte, true);
          last_sent_byte_ = first_byte;
          session_state_ = SessionState::response;
          last_state_change_ = Clock::now();
          work_done = true;
        } else {
          last_error_message_ = "Client sent no data for bus request.";
          std::cout << "[ClientManager] Client fd=" << active_sender->getFd()
                    << " sent no data for bus request" << std::endl;
          stopActiveSession();
          work_done = true;
        }
      }
    } else if (session_state_ == SessionState::transmit) {
      uint8_t send_byte = 0;
      if (active_sender->popPendingBusRequest(send_byte)) {
        bus_->writeByte(send_byte);
        last_sent_byte_ = send_byte;
        platform::LockGuard<platform::Mutex> lock(mutex_);
        session_state_ = SessionState::response;
        last_state_change_ = Clock::now();
        work_done = true;
      } else {
        stopActiveSession();
        work_done = true;
      }
    }
  }

  return work_done;
}

void ClientManager::processClientIoEvent(int client_fd, uint16_t events) {
  auto client = findClientByFd(client_fd);
  if (!client) {
    // Already removed by another path (e.g. removeDisconnectedClients)
    return;
  }

  // Handle error/disconnect
  if (events & io_err) {
    std::cout << "[ClientManager] Socket error on client fd=" << client_fd
              << std::endl;
    removeClientByFd(client_fd);
    return;
  }

  // Handle input
  if (events & io_in) {
    // Peek to detect EOF
    char dummy_buf[1];
    ssize_t n = platform::recv(client_fd, dummy_buf, 1, platform::Flags::peek);
    if (n == 0) {
      std::cout << "[ClientManager] Client fd=" << client_fd
                << " disconnected (EOF)" << std::endl;
      removeClientByFd(client_fd);
      return;
    } else if (n < 0 && !platform::isWouldBlock() &&
               !platform::isInterrupted()) {
      std::cout << "[ClientManager] Client fd=" << client_fd
                << " recv error errno=" << errno << std::endl;
      removeClientByFd(client_fd);
      return;
    }

    // Read available data
    uint8_t buffer[256];
    while (true) {
      ssize_t bytes_read = platform::recv(client_fd, buffer, sizeof(buffer),
                                          platform::Flags::dont_wait);
      if (bytes_read > 0) {
        client->processIncomingData(buffer, static_cast<size_t>(bytes_read));
      } else if (bytes_read == 0) {
        std::cout << "[ClientManager] Client fd=" << client_fd
                  << " closed by peer" << std::endl;
        removeClientByFd(client_fd);
        return;
      } else {
        if (platform::isWouldBlock() || platform::isInterrupted()) {
          break;
        } else {
          std::cout << "[ClientManager] Client fd=" << client_fd
                    << " read error errno=" << errno << std::endl;
          removeClientByFd(client_fd);
          return;
        }
      }
    }
  }

  // Handle output
  if (events & io_out) {
    if (!client->isConnected()) {
      removeClientByFd(client_fd);
      return;
    }
    client->tryFlushOutboundBuffer();
  }
}

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
    for (auto& c : regular_clients_)
      if (c) snapshot.push_back(c->getClientInfo());
    for (auto& c : readonly_clients_)
      if (c) snapshot.push_back(c->getClientInfo());
    for (auto& c : enhanced_clients_)
      if (c) snapshot.push_back(c->getClientInfo());
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

void ClientManager::onExternalBusRequested() {
  bus_requested_.store(true, std::memory_order_release);
}

void ClientManager::onBusEvent(const BusEventInfo& info) {
  // Collect all connected clients
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      all_clients;
  std::shared_ptr<AbstractClient> active_sender;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (!running_.load(std::memory_order_acquire)) return;
    active_sender = current_active_sender_;

    // Snapshot all connected clients
    for (auto& c : regular_clients_)
      if (c && c->isConnected()) all_clients.push_back(c);
    for (auto& c : readonly_clients_)
      if (c && c->isConnected()) all_clients.push_back(c);
    for (auto& c : enhanced_clients_)
      if (c && c->isConnected()) all_clients.push_back(c);
  }

  // Handle active sender's onBusByte
  if (active_sender && active_sender->isConnected()) {
    BridgeAction action = BridgeAction::keep_active;
    action = active_sender->onBusByte(info);

    if (action != BridgeAction::keep_active) {
      platform::UniqueLock<platform::Mutex> lock(mutex_);
      if (action == BridgeAction::stop_session) {
        if (monitor_)
          monitor_->updateRequest([](auto& m) { m.bus_request_blocked++; });
        lock.unlock();
        stopActiveSession();
      } else if (action == BridgeAction::bypass_wait) {
        session_state_ = SessionState::transmit;
        last_state_change_ = Clock::now();
        bus_requested_.store(false, std::memory_order_release);
      }
    }
  }

  // Forward byte to all clients except active sender
  for (auto& client : all_clients) {
    if (client && client->isConnected() && client != active_sender) {
      client->sendToClient(ByteView(&info.byte, 1));
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
    bus_requested_.store(false, std::memory_order_release);
    last_error_message_ = "Session stopped by ClientManager.";
    std::cout << "[ClientManager] Stopping session for client fd="
              << old_sender->getFd() << std::endl;
    if (permit_) {
      permit_->release(BusAccessPermit::external);
      std::cout << "[ClientManager] Bus permit released" << std::endl;
    }
  }
  if (old_sender) old_sender->stop();
  if (request_) request_->reset();
}

std::shared_ptr<AbstractClient> ClientManager::findClientByFdLocked(int fd) {
  for (auto& client : regular_clients_) {
    if (client && client->getFd() == fd) return client;
  }
  for (auto& client : readonly_clients_) {
    if (client && client->getFd() == fd) return client;
  }
  for (auto& client : enhanced_clients_) {
    if (client && client->getFd() == fd) return client;
  }
  return nullptr;
}

std::shared_ptr<AbstractClient> ClientManager::findClientByFd(int fd) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return findClientByFdLocked(fd);
}

void ClientManager::removeClientByFd(int fd) {
  std::shared_ptr<AbstractClient> client;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    client = findClientByFdLocked(fd);
    if (!client) return;

    // Find and remove from the appropriate array
    for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
      if (regular_clients_[i] == client) {
        regular_clients_[i].reset();
        break;
      }
    }
    for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
      if (readonly_clients_[i] == client) {
        readonly_clients_[i].reset();
        break;
      }
    }
    for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
      if (enhanced_clients_[i] == client) {
        enhanced_clients_[i].reset();
        break;
      }
    }
  }

  // Stop client outside lock to avoid holding mutex_ during close() block/delay
  if (client) {
    std::cout << "[ClientManager] Removing client fd=" << fd << std::endl;
    client->stop();
  }
}

void ClientManager::removeDisconnectedClients() {
  // Collect disconnected clients under the lock, then stop them outside
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients * 3>
      to_stop;

  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    bool removed = false;

    auto cleanup = [&](auto& arr, const char* type_name) {
      for (size_t i = 0; i < NetworkLimits::max_clients; ++i) {
        if (arr[i] && !arr[i]->isConnected()) {
          std::cout << "[ClientManager] Removing disconnected " << type_name
                    << " client fd=" << arr[i]->getFd() << " slot=" << i
                    << std::endl;
          to_stop.push_back(arr[i]);
          arr[i].reset();
          removed = true;
        }
      }
    };

    cleanup(regular_clients_, "regular");
    cleanup(readonly_clients_, "readonly");
    cleanup(enhanced_clients_, "enhanced");

    if (removed) {
      signalClientIoThread();
    }
  }

  // Stop sockets outside the lock
  for (auto& c : to_stop) {
    c->stop();
  }
}

void ClientManager::clientIoLoop() {
  while (client_io_running_.load()) {
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int max_fd = -1;

    // Add wakeup signal FD
    int wakeup_fd = wakeup_signal_.getReadFd();
    if (wakeup_fd >= 0) {
      FD_SET(wakeup_fd, &readfds);
      max_fd = wakeup_fd;
    }

    // Add server listening sockets & client fds to select
    {
      platform::LockGuard<platform::Mutex> lock(mutex_);
      auto setup_listeners =
          [&](const std::unique_ptr<platform::Socket>& listener) {
            if (!listener || !listener->isValid()) return;

            int listen_fd = listener->getFd();
            FD_SET(listen_fd, &readfds);
            if (listen_fd > max_fd) max_fd = listen_fd;
          };

      setup_listeners(listen_socket_regular_);
      setup_listeners(listen_socket_readonly_);
      setup_listeners(listen_socket_enhanced_);

      auto setup_clients = [&](const auto& client_array) {
        for (auto& c : client_array) {
          if (c && c->isConnected()) {
            int fd = c->getFd();
            FD_SET(fd, &readfds);
            FD_SET(fd, &exceptfds);
            if (c->hasPendingData()) FD_SET(fd, &writefds);
            if (fd > max_fd) max_fd = fd;
          }
        }
      };

      setup_clients(regular_clients_);
      setup_clients(readonly_clients_);
      setup_clients(enhanced_clients_);
    }

    if (max_fd == -1) {
      platform::sleepMilli(100);
      continue;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms

    int activity = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);

    if (!client_io_running_.load()) break;
    if (activity < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (activity == 0) continue;

    // Drain wakeup signal
    if (wakeup_fd >= 0 && FD_ISSET(wakeup_fd, &readfds)) {
      wakeup_signal_.drain();
    }

    // Handle new connections
    ebus::StaticVector<std::pair<int, ClientType>, NetworkLimits::max_clients>
        pending_clients;
    {
      platform::LockGuard<platform::Mutex> lock(mutex_);

      auto handle_accept = [&](std::unique_ptr<platform::Socket>& listener,
                               ClientType type) {
        if (!listener || !listener->isValid()) return false;

        int listen_fd = listener->getFd();
        if (!FD_ISSET(listen_fd, &readfds)) {
          return false;
        }

        int client_fd = listener->accept();
        if (client_fd >= 0) {
          std::cout << "[ClientManager] Connection accepted on listener fd "
                    << listen_fd << " -> client fd " << client_fd << " (type "
                    << static_cast<int>(type) << ")" << std::endl;
          pending_clients.emplace_back(client_fd, type);
          return true;
        } else {
          std::cout << "[ClientManager] ERROR: accept() failed on listener fd "
                    << listen_fd << ", errno: " << errno << std::endl;
        }
        return false;
      };

      handle_accept(listen_socket_regular_, ClientType::regular);
      handle_accept(listen_socket_readonly_, ClientType::read_only);
      handle_accept(listen_socket_enhanced_, ClientType::enhanced);
    }

    // Add pending clients outside of the lock to avoid holding mutex_ during
    // addClient()
    for (const auto& client_pair : pending_clients) {
      int client_fd = client_pair.first;
      ClientType type = client_pair.second;
      if (!addClient(client_fd, type)) {
        std::cout << "[ClientManager] ERROR: Failed to register client fd "
                  << client_fd << std::endl;
        platform::close(client_fd);
      }
    }

    // Collect readiness events for active clients
    {
      platform::LockGuard<platform::Mutex> lock(mutex_);
      auto process_clients = [&](const auto& client_array) {
        for (auto& c : client_array) {
          if (c && c->isConnected()) {
            int fd = c->getFd();
            uint16_t events = 0;
            if (FD_ISSET(fd, &readfds)) events |= io_in;
            if (FD_ISSET(fd, &exceptfds)) events |= io_err;
            if (c->hasPendingData()) {
              FD_SET(fd, &writefds);
              events |= io_out;
            }
            if (events != 0 && reactor_queue_) {
              OrchestrationEvent ev;
              ev.type = OrchestrationEventType::client_io_ready;
              ev.data.client_io_data.client_fd = fd;
              ev.data.client_io_data.events = events;
              reactor_queue_->tryPush(std::move(ev));
            }
          }
        }
      };

      process_clients(regular_clients_);
      process_clients(readonly_clients_);
      process_clients(enhanced_clients_);
    }
  }
}

void ClientManager::signalClientIoThread() { wakeup_signal_.signal(); }

}  // namespace ebus::detail
