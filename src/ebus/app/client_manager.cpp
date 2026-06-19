/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client_manager.hpp"

#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/utils.hpp>
#include <iterator>

#include "core/bus_monitor.hpp"
#include "platform/socket.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

static int createListenSocket(uint16_t port) {
  int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) return -1;

  int enable = 1;
  ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    platform::close(listen_fd);
    return -1;
  }

  if (::listen(listen_fd, 4) != 0) {
    platform::close(listen_fd);
    return -1;
  }

  platform::setNonBlocking(listen_fd);
  return listen_fd;
}

ClientManager::ClientManager(
    platform::Bus* bus, BusHandler* bus_handler, Request* request,
    BusMonitor* monitor,
    detail::platform::Queue<OrchestrationEvent>* reactor_queue)
    : bus_(bus),
      bus_handler_(bus_handler),
      request_(request),
      monitor_(monitor),
      reactor_queue_(reactor_queue),
      running_(false),
      session_state_(SessionState::idle),
      last_state_change_(Clock::now()),
      clients_version_(0),
      last_snapshot_version_(0),
      session_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.session_timeout_ms)),
      transmit_timeout_(::std::chrono::milliseconds(
          ebus::RuntimeConfig{}.network.transmit_timeout_ms)),
      outbound_buffer_size_(
          ebus::RuntimeConfig{}.network.outbound_buffer_size) {
  assert(bus_ != nullptr && "Bus pointer cannot be null");
  assert(request_ != nullptr && "Request pointer cannot be null");
  assert(reactor_queue_ != nullptr && "Reactor queue pointer cannot be null");

  if (!wakeup_signal_.init()) {
    assert(false && "Failed to create WakeupSignal for ClientManager");
  }

  if (bus_handler_) {
    bus_listener_id_ = bus_handler_->addByteListener(
        Delegate<void(const BusEventInfo&)>::bind<
            ClientManager, &ClientManager::handleBusEvent>(this));
  }

  if (request_) {
    request_->setExternalBusRequestedCallback(
        Delegate<void()>::bind<ClientManager,
                               &ClientManager::onExternalBusRequested>(this));
  }
}

ClientManager::~ClientManager() {
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
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    if (config.network.enable_server) {
      listen_fd_regular_ = createListenSocket(config.network.port_regular);
      listen_fd_readonly_ = createListenSocket(config.network.port_readonly);
      listen_fd_enhanced_ = createListenSocket(config.network.port_enhanced);
    }
  }

  client_io_running_.store(true);
  client_io_worker_ = std::make_unique<platform::ServiceThread>(
      "ebus_client_manager", [this] { clientIoLoop(); },
      OrchestrationLimits::default_stack_size,
      OrchestrationLimits::default_priority);
  client_io_worker_->start();

  running_.store(true, std::memory_order_release);
}

void ClientManager::stop() {
  running_.store(false, std::memory_order_release);
  client_io_running_.store(false);
  signalClientIoThread();  // Signal the IO thread to wake up and exit
  if (client_io_worker_) {
    client_io_worker_->join();
    client_io_worker_.reset();
  }

  {
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    if (listen_fd_regular_ >= 0) {
      platform::close(listen_fd_regular_);
      listen_fd_regular_ = -1;
    }
    if (listen_fd_readonly_ >= 0) {
      platform::close(listen_fd_readonly_);
      listen_fd_readonly_ = -1;
    }
    if (listen_fd_enhanced_ >= 0) {
      platform::close(listen_fd_enhanced_);
      listen_fd_enhanced_ = -1;
    }
  }
}

void ClientManager::setSessionTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  // Ensure the IO thread is aware of potential changes that might affect its
  // polling behavior
  signalClientIoThread();
  session_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setTransmitTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  transmit_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void ClientManager::setOutboundBufferSize(size_t size) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  // This might affect client creation, so signal the IO thread to rebuild its
  // list
  signalClientIoThread();
  outbound_buffer_size_ = size;
}

void ClientManager::setBusyPredicate(Delegate<bool()> pred) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  is_busy_ = std::move(pred);
}

bool ClientManager::addClient(int fd, ClientType type) {
  auto client = createClient(fd, request_, type, outbound_buffer_size_);
  if (!client) return false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (clients_.size() >= NetworkLimits::max_clients) {
      client->stop();
      return false;
    }
    clients_.push_back(std::move(client));
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    // Add to client_io_map_
    if (fd >= 0) {
      if (client_io_map_.full()) {
        // This should ideally not happen if max_clients is respected
        assert(false && "client_io_map_ is full");
      }
      client_io_map_.emplace_back(fd, clients_.back());
    }
    ++clients_version_;
  }
  return true;
}

bool ClientManager::addClient(std::shared_ptr<AbstractClient> client) {
  if (!client) return false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (clients_.size() >= NetworkLimits::max_clients) {
      client->stop();
      return false;
    }
    int fd = client->getFd();  // Capture the FD before moving the pointer
    clients_.push_back(std::move(client));
    // Add to client_io_map_
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    if (fd >= 0) {
      if (client_io_map_.full()) {
        // This should ideally not happen if max_clients is respected
        assert(false && "client_io_map_ is full");
      }
      client_io_map_.emplace_back(fd, clients_.back());
    }
    ++clients_version_;
  }
  return true;
}

void ClientManager::removeClient(int fd) {
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                       [fd](const std::shared_ptr<AbstractClient>& c) {
                         return c->getFd() == fd;
                       }),
        clients_.end());
    ++clients_version_;
    // Remove from client_io_map_
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    client_io_map_.erase(
        std::remove_if(client_io_map_.begin(), client_io_map_.end(),
                       [fd](const auto& pair) { return pair.first == fd; }),
        client_io_map_.end());
  }
  signalClientIoThread();  // Signal IO thread to update its pollfd list
}

void ClientManager::removeDisconnectedClients() {
  bool clients_removed = false;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    auto it = std::remove_if(clients_.begin(), clients_.end(),
                             [](const std::shared_ptr<AbstractClient>& c) {
                               return !c->isConnected();
                             });
    if (it != clients_.end()) {
      clients_.erase(it, clients_.end());
      clients_removed = true;
      ++clients_version_;
    }
  }

  if (clients_removed) {
    // Rebuild client_io_map_ efficiently
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    client_io_map_.clear();
    for (const auto& client : clients_) {
      int fd = client->getFd();
      if (fd >= 0) {
        client_io_map_.emplace_back(fd, client);
      }
    }
    signalClientIoThread();
  }
}

bool ClientManager::tick() {
  if (!running_.load(std::memory_order_acquire)) return false;
  bool work_done = false;

  // Proactively remove disconnected clients from the master list
  removeDisconnectedClients();

  // 1. Snapshot clients only if changed
  ebus::StaticVector<std::shared_ptr<AbstractClient>,
                     NetworkLimits::max_clients>
      active_clients;
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (clients_version_ != last_snapshot_version_) {
      clients_cache_.clear();
      std::copy(clients_.begin(), clients_.end(),
                std::back_inserter(clients_cache_));
      last_snapshot_version_ = clients_version_;
    }
    std::copy(clients_cache_.begin(), clients_cache_.end(),
              std::back_inserter(active_clients));
  }

  // 2. Housekeeping & select active sender
  {
    platform::UniqueLock<platform::Mutex> lock(mutex_);
    if (current_active_sender_ && !current_active_sender_->isConnected()) {
      auto old = current_active_sender_;
      current_active_sender_.reset();
      session_state_ = SessionState::idle;
      bus_requested_.store(false, std::memory_order_release);
      last_error_message_ = "Client disconnected.";
      lock.unlock();
      old->stop();
      request_->reset();
      lock.lock();
      work_done = true;
    }

    if (!current_active_sender_ && session_state_ == SessionState::idle &&
        (!is_busy_ || !is_busy_())) {
      for (auto& client : active_clients) {
        if (client->isConnected() && client->isWriteCapable() &&
            client->wantsToSend()) {
          current_active_sender_ = client;
          client->onSessionStart(++session_counter_);
          session_state_ = SessionState::request;
          last_state_change_ = Clock::now();
          bus_requested_.store(false, std::memory_order_release);
          work_done = true;
          break;
        }
      }
    }
  }

  // 3. Outbound logic
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
      stopActiveSession();
      work_done = true;
      if (monitor_)
        monitor_->updateRequest([](auto& m) { m.session_timeouts++; });
      last_error_message_ = "Session timed out.";
    } else if (session_state_ == SessionState::request) {
      if (request_->busAvailable()) {
        uint8_t first_byte = 0;
        if (active_sender->recvFromClient(first_byte)) {
          platform::LockGuard<platform::Mutex> lock(mutex_);
          request_->requestBus(first_byte, true);
          last_sent_byte_ = first_byte;
          session_state_ = SessionState::response;
          last_state_change_ = Clock::now();
          work_done = true;
        } else {
          last_error_message_ = "Client sent no data for bus request.";
          stopActiveSession();
          work_done = true;
        }
      }
    } else if (session_state_ == SessionState::transmit) {
      uint8_t send_byte = 0;
      if (active_sender->recvFromClient(send_byte)) {
        bus_->writeByte(send_byte);
        last_sent_byte_ = send_byte;
        platform::LockGuard<platform::Mutex> lock(mutex_);
        session_state_ = SessionState::response;
        last_state_change_ = Clock::now();
        work_done = true;
      }
    }
  }

  return work_done;
}

void ClientManager::processClientIoEvent(int client_fd, uint16_t events) {
  std::shared_ptr<AbstractClient> client;
  {
    platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
    auto it = std::find_if(
        client_io_map_.begin(), client_io_map_.end(),
        [client_fd](const auto& pair) { return pair.first == client_fd; });
    if (it != client_io_map_.end()) {
      client = it->second;
    }
  }

  if (!client) return;  // Client might have been removed

  if (events & io_err) {
    client->stop();
    signalClientIoThread();  // Signal to rebuild pollfds and main thread to
                             // remove
    return;
  }

  if (events & io_in) {
    // Attempt to peek 1 byte to detect EOF (graceful disconnection) without
    // consuming data
    char dummy_buf[1];
    ssize_t n = platform::recv(client_fd, dummy_buf, 1, platform::Flags::peek);
    if (n == 0) {  // EOF detected, client disconnected gracefully
      client->stop();
      signalClientIoThread();
      return;
    } else if (n < 0 && !platform::isWouldBlock() &&
               !platform::isInterrupted()) {
      // An actual read error (not just would block or interrupted)
      client->stop();
      signalClientIoThread();
      return;
    }
  }

  if (events & io_out) {
    client->tryFlushOutboundBuffer();
  }
}

void ClientManager::handleBusEvent(const BusEventInfo& info) {
  platform::UniqueLock<platform::Mutex> lock(mutex_);
  if (!running_.load(std::memory_order_acquire)) return;

  if (current_active_sender_) {
    if (session_state_ != SessionState::idle) {
      auto action = current_active_sender_->onBusByte(info);
      if (action == BridgeAction::stop_session) {
        if (monitor_)
          monitor_->updateRequest([](auto& m) { m.bus_request_blocked++; });
        lock.unlock();
        stopActiveSession();
        lock.lock();
      } else if (action == BridgeAction::bypass_wait) {
        session_state_ = SessionState::transmit;
        last_state_change_ = Clock::now();
        bus_requested_.store(false, std::memory_order_release);
      }
    }
  }

  for (auto& client : clients_cache_) {
    if (client != current_active_sender_ && client->isConnected()) {
      client->sendToClient(ByteView(&info.byte, 1));
    }
  }
}

void ClientManager::signalClientIoThread() { wakeup_signal_.signal(); }

void ClientManager::clientIoLoop() {
  while (client_io_running_.load()) {
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int max_fd = -1;

    // 1. Add wakeup signal FD
    int wakeup_fd = wakeup_signal_.getReadFd();
    if (wakeup_fd >= 0) {
      FD_SET(wakeup_fd, &readfds);
      max_fd = wakeup_fd;
    }

    // 2. Add server listening sockets & client descriptors
    {
      platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
      if (listen_fd_regular_ >= 0) {
        FD_SET(listen_fd_regular_, &readfds);
        if (listen_fd_regular_ > max_fd) max_fd = listen_fd_regular_;
      }
      if (listen_fd_readonly_ >= 0) {
        FD_SET(listen_fd_readonly_, &readfds);
        if (listen_fd_readonly_ > max_fd) max_fd = listen_fd_readonly_;
      }
      if (listen_fd_enhanced_ >= 0) {
        FD_SET(listen_fd_enhanced_, &readfds);
        if (listen_fd_enhanced_ > max_fd) max_fd = listen_fd_enhanced_;
      }

      for (const auto& pair : client_io_map_) {
        if (pair.second->isConnected()) {
          int fd = pair.first;
          FD_SET(fd, &readfds);
          FD_SET(fd, &exceptfds);

          // Optimization for single-core ESP32: only monitor write readiness
          // if the client actually has data pending in its outbound buffer.
          // This prevents select from returning constantly for idle sockets.
          if (pair.second->hasPendingData()) {
            FD_SET(fd, &writefds);
          }

          if (fd > max_fd) max_fd = fd;
        }
      }
    }

    if (max_fd == -1) {
      platform::sleepMilli(100);
      continue;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms blocking window

    int activity = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);

    if (!client_io_running_.load()) break;

    if (activity < 0) {
      if (errno == EINTR) continue;  // Interrupted by signal, retry
      break;                         // Fatal error
    }

    if (activity == 0) continue;  // Idle timeout

    // Drain wakeup signal
    if (wakeup_fd >= 0 && FD_ISSET(wakeup_fd, &readfds)) {
      wakeup_signal_.drain();
    }

    // Handle new server connections without holding client_io_mutex_ while
    // calling addClient().
    ebus::StaticVector<std::pair<int, ClientType>, NetworkLimits::max_clients>
        pending_clients;
    {
      platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);

      auto try_accept = [&](int listen_fd, ClientType type) {
        sockaddr_in addr{};
        socklen_t addr_len = sizeof(addr);
        int client_fd =
            ::accept(listen_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (client_fd >= 0) {
          pending_clients.emplace_back(client_fd, type);
        }
      };

      if (listen_fd_regular_ >= 0 && FD_ISSET(listen_fd_regular_, &readfds)) {
        try_accept(listen_fd_regular_, ClientType::regular);
      }
      if (listen_fd_readonly_ >= 0 && FD_ISSET(listen_fd_readonly_, &readfds)) {
        try_accept(listen_fd_readonly_, ClientType::read_only);
      }
      if (listen_fd_enhanced_ >= 0 && FD_ISSET(listen_fd_enhanced_, &readfds)) {
        try_accept(listen_fd_enhanced_, ClientType::enhanced);
      }
    }

    for (const auto& client_pair : pending_clients) {
      int client_fd = client_pair.first;
      ClientType type = client_pair.second;
      platform::setNonBlocking(client_fd);
      int flag = 1;
      ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
      if (!addClient(client_fd, type)) {
        platform::close(client_fd);
      }
    }

    // Collect readiness events for active clients
    {
      platform::LockGuard<platform::Mutex> io_lock(client_io_mutex_);
      for (const auto& pair : client_io_map_) {
        int fd = pair.first;
        uint16_t events = 0;
        if (FD_ISSET(fd, &readfds)) events |= io_in;
        if (FD_ISSET(fd, &writefds)) events |= io_out;
        if (FD_ISSET(fd, &exceptfds)) events |= io_err;

        if (events != 0 && reactor_queue_) {
          OrchestrationEvent ev;
          ev.type = OrchestrationEventType::client_io_ready;
          ev.data.client_io_data.client_fd = fd;
          ev.data.client_io_data.events = events;
          reactor_queue_->tryPush(std::move(ev));
        }
      }
    }
  }
}

Clock::time_point ClientManager::nextDueTime() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (!running_.load(std::memory_order_acquire))
    return Clock::time_point::max();

  if (current_active_sender_) {
    auto current_timeout = (session_state_ == SessionState::transmit)
                               ? transmit_timeout_
                               : session_timeout_;
    return last_state_change_ + current_timeout;
  }

  // Starvation Fix: If the system is busy, the ClientManager should not
  // request a wakeup "now", as it would lead to a tight spin loop.
  if (is_busy_ && is_busy_()) {
    return Clock::time_point::max();
  }

  return Clock::now() +
         std::chrono::milliseconds(
             100);  // Wake up periodically to check for new clients/timeouts
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

  platform::LockGuard<platform::Mutex> lock(mutex_);
  ClientManagerStatus s{map(getThreadStatus()),
                        current_active_sender_ != nullptr,
                        ebus::toString(session_state_), last_error_message_};

  for (const auto& client : clients_) {
    if (client) {
      s.clients.push_back(client->getClientInfo());
    }
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
  }
  // do work outside lock
  if (old_sender) old_sender->stop();
  if (request_) request_->reset();
}

}  // namespace ebus::detail
