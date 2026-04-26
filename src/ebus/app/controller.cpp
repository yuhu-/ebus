/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <deque>
#include <ebus/controller.hpp>
#include <ebus/utils.hpp>

#include "app/client_manager.hpp"
#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/service_thread.hpp"
#include "platform/system.hpp"
#include "utils/circular_buffer.hpp"

namespace ebus {

struct Impl {
  mutable std::mutex error_mutex_;
  detail::CircularBuffer<ebus::ErrorEntry> error_buffer_{
      defaults::Logging::log_size};
  ebus::ReactiveMasterSlaveCallback user_reactive_callback_;
  ebus::TelegramCallback user_telegram_callback_;
  ebus::ErrorCallback user_error_callback_;
  std::unique_ptr<detail::Request> request_;
  std::unique_ptr<detail::BusMonitor> bus_monitor_;
  std::unique_ptr<detail::Bus> bus_;
  std::unique_ptr<detail::BusHandler> bus_handler_;
  std::unique_ptr<detail::Handler> handler_;
  std::unique_ptr<detail::DeviceManager> device_manager_;
  std::unique_ptr<detail::DeviceScanner> device_scanner_;
  std::unique_ptr<detail::PollManager> poll_manager_;
  std::unique_ptr<detail::Scheduler> scheduler_;
  std::unique_ptr<detail::ClientManager> client_manager_;
  std::unique_ptr<detail::ServiceThread> worker_;
};

Controller::Controller() : impl_(new Impl()) {}

Controller::Controller(const EbusConfig& config)
    : config_(config), impl_(new Impl()) {
  configured_ = true;
  constructMembers();
}

Controller::~Controller() { stop(); }

void Controller::configure(const EbusConfig& config) {
  if (running_) return;
  config_ = config;
  configured_ = true;
  constructMembers();
}

void Controller::start() {
  if (!configured_) return;
  if (running_) return;
  running_ = true;
  impl_->bus_->start();
  impl_->bus_handler_->start();
  impl_->scheduler_->start();
  impl_->client_manager_->start();
  impl_->worker_ = std::make_unique<detail::ServiceThread>(
      "ebusController", [this] { run(); }, detail::Orchestration::stack_size,
      detail::Orchestration::priority_low, 0);
  impl_->worker_->start();
}

void Controller::stop() {
  if (!configured_) return;
  if (!running_) return;
  running_ = false;
  if (impl_->worker_) impl_->worker_->join();
  impl_->client_manager_->stop();
  impl_->scheduler_->stop();
  impl_->bus_handler_->stop();
  impl_->bus_->stop();
}

void Controller::setAddress(const uint8_t& address) {
  config_.runtime.address = address;
  if (configured_) {
    impl_->handler_->setSourceAddress(address);
    impl_->device_manager_->setOwnAddress(address);
    impl_->device_scanner_->setOwnAddress(address);
    impl_->bus_->setRuntimeConfig(config_.runtime);
    impl_->request_->setMaxLockCounter(
        config_.runtime.arbitration.lock_counter_max);
  }
}

void Controller::setWindow(const uint16_t& window) {
  config_.runtime.bus.timing.window = window;
  if (configured_) impl_->bus_->setWindow(window);
}

void Controller::setOffset(const uint16_t& offset) {
  config_.runtime.bus.timing.offset = offset;
  if (configured_) impl_->bus_->setOffset(offset);
}

void Controller::setErrorLogSize(size_t size) {
  config_.runtime.logging.log_size = size;
  if (configured_) {
    std::lock_guard<std::mutex> lock(impl_->error_mutex_);
    impl_->error_buffer_.set_capacity(size);
  }
}

void Controller::setClientActiveTimeout(std::chrono::milliseconds timeout) {
  config_.runtime.network.timing.client_timeout = timeout;
  if (impl_->client_manager_) {
    impl_->client_manager_->setActiveTimeout(timeout);
  }
}

void Controller::setOutboundBufferSize(size_t size) {
  config_.runtime.network.outbound_buffer_size = size;
  if (impl_->client_manager_) {
    impl_->client_manager_->setOutboundBufferSize(size);
  }
}

void Controller::setMaxSendAttempts(int max_send_attempts) {
  config_.runtime.scheduler.max_send_attempts = max_send_attempts;
  if (impl_->scheduler_) {
    impl_->scheduler_->setMaxSendAttempts(max_send_attempts);
  }
}

void Controller::setBaseBackoff(std::chrono::milliseconds base_backoff) {
  config_.runtime.scheduler.timing.base_backoff = base_backoff;
  if (impl_->scheduler_) impl_->scheduler_->setBaseBackoff(base_backoff);
}

void Controller::setFsmTimeout(std::chrono::milliseconds timeout) {
  config_.runtime.scheduler.timing.fsm_timeout = timeout;
  if (impl_->scheduler_) {
    impl_->scheduler_->setFsmTimeout(timeout);
  }
}

void Controller::setWatchdogTimeout(std::chrono::milliseconds timeout) {
  config_.runtime.network.timing.watchdog_timeout = timeout;
  if (impl_->bus_handler_) {
    impl_->bus_handler_->setWatchdogTimeout(timeout);
  }
}

void Controller::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  impl_->user_reactive_callback_ = std::move(callback);
  if (impl_->scheduler_) {
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
  }
}

void Controller::setTelegramCallback(TelegramCallback callback) {
  impl_->user_telegram_callback_ = std::move(callback);
}

void Controller::setErrorCallback(ErrorCallback callback) {
  impl_->user_error_callback_ = std::move(callback);
}

void Controller::enqueue(uint8_t priority, ByteView message,
                         ResultCallback callback) {
  if (impl_->scheduler_)
    impl_->scheduler_->enqueue(priority, message, std::move(callback));
}

uint32_t Controller::addPollItem(uint8_t priority, ByteView message,
                                 std::chrono::milliseconds interval,
                                 ResultCallback callback) {
  uint32_t id = impl_->poll_manager_
                    ? impl_->poll_manager_->addPollItem(
                          priority, message, interval, std::move(callback))
                    : 0;
  wake_cv_.notify_one();
  return id;
}

void Controller::removePollItem(uint32_t id) {
  if (impl_->poll_manager_) impl_->poll_manager_->removePollItem(id);
}

void Controller::setFullScan(bool enable) {
  if (impl_->device_scanner_) impl_->device_scanner_->setFullScan(enable);
}

void Controller::setScanOnStartup(bool enable) {
  if (impl_->device_scanner_) impl_->device_scanner_->setScanOnStartup(enable);
}

void Controller::scanAddress(uint8_t address) {
  if (impl_->device_scanner_) impl_->device_scanner_->scanAddress(address);
  wake_cv_.notify_one();
}

void Controller::scanObservedDevices() {
  if (impl_->device_scanner_) impl_->device_scanner_->scanObservedDevices();
}

bool Controller::isScanning() const {
  return impl_->device_scanner_ ? impl_->device_scanner_->isScanning() : false;
}

void Controller::addClient(int fd, ClientType type) {
  if (impl_->client_manager_) impl_->client_manager_->addClient(fd, type);
}

void Controller::removeClient(int fd) {
  if (impl_->client_manager_) impl_->client_manager_->removeClient(fd);
}

std::vector<DeviceInfo> Controller::getDeviceInfo() const {
  if (!impl_->device_manager_) return {};
  return impl_->device_manager_->getDeviceInfo();
}

std::string Controller::getDeviceInfoJson() const {
  if (!impl_->device_manager_) return "[]";
  return impl_->device_manager_->getDeviceInfoJson();
}

void Controller::resetMetrics() {
  if (impl_->bus_monitor_) impl_->bus_monitor_->resetMetrics();
}

Metrics Controller::getMetrics() const {
  return (configured_ && impl_->bus_monitor_)
             ? impl_->bus_monitor_->getMetrics()
             : Metrics{};
}

std::vector<ErrorEntry> Controller::getErrors() const {
  std::lock_guard<std::mutex> lock(impl_->error_mutex_);
  if (impl_->error_buffer_.empty()) return {};

  std::vector<ErrorEntry> result;
  result.reserve(impl_->error_buffer_.size());

  // CircularBuffer provides chronological indexing internally
  for (size_t i = 0; i < impl_->error_buffer_.size(); ++i) {
    result.push_back(impl_->error_buffer_[i]);
  }
  return result;
}

std::string Controller::getErrorsJson() const { return toJson(getErrors()); }

size_t Controller::getErrorLogCapacity() const {
  std::lock_guard<std::mutex> lock(impl_->error_mutex_);
  return impl_->error_buffer_.capacity();
}

void Controller::clearErrors() {
  std::lock_guard<std::mutex> lock(impl_->error_mutex_);
  impl_->error_buffer_.clear();
}

bool Controller::isConfigured() const noexcept { return configured_; }

bool Controller::isRunning() const noexcept { return running_; }

void Controller::constructMembers() {
  impl_->bus_monitor_ = std::make_unique<detail::BusMonitor>();
  impl_->request_ =
      std::make_unique<detail::Request>(impl_->bus_monitor_.get());
  impl_->request_->setMaxLockCounter(
      config_.runtime.arbitration.lock_counter_max);
  impl_->bus_ = std::make_unique<detail::Bus>(config_.bus, config_.runtime,
                                              impl_->request_.get(),
                                              impl_->bus_monitor_.get());
  impl_->handler_ = std::make_unique<detail::Handler>(
      config_.runtime.address, impl_->bus_.get(), impl_->request_.get(),
      impl_->bus_monitor_.get());

  impl_->scheduler_ =
      std::make_unique<detail::Scheduler>(impl_->handler_.get());
  impl_->scheduler_->setMaxSendAttempts(
      config_.runtime.scheduler.max_send_attempts);
  impl_->scheduler_->setBaseBackoff(
      config_.runtime.scheduler.timing.base_backoff);
  impl_->scheduler_->setFsmTimeout(
      config_.runtime.scheduler.timing.fsm_timeout);

  impl_->device_manager_ =
      std::make_unique<detail::DeviceManager>(impl_->bus_monitor_.get());
  impl_->device_manager_->setOwnAddress(config_.runtime.address);

  if (impl_->user_reactive_callback_) {
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
  }

  // Setup the central dispatcher via the Scheduler.
  // The Scheduler handles the timing-critical Handler interaction and
  // provides an 'extern' callback hook which we use to feed the rest of the
  // app.
  impl_->scheduler_->setTelegramCallback([this](const TelegramInfo& info) {
    // 1. Update Internal State (Device Discovery)
    if (impl_->device_manager_) {
      impl_->device_manager_->update(info.master_view, info.slave_view);
    }
    // 2. Inform the Application (Active, Passive, and Reactive)
    if (impl_->user_telegram_callback_) {
      impl_->user_telegram_callback_(info);
    }
  });

  impl_->scheduler_->setErrorCallback([this](const ErrorInfo& info) {
    // 1. Update internal circular buffer
    {
      std::lock_guard<std::mutex> lock(impl_->error_mutex_);
      if (config_.runtime.logging.log_size > 0) {
        ErrorEntry entry;
        entry.level = info.level;
        entry.setMessage(info.message);
        entry.result = info.result;
        entry.handler_state = info.handler_state;
        entry.request_state = info.request_state;
        entry.setMaster(info.master_view.data(), info.master_view.size());
        entry.setSlave(info.slave_view.data(), info.slave_view.size());
        entry.utilization = info.utilization;
        entry.timestamp = std::chrono::system_clock::now();

        impl_->error_buffer_.push_back(std::move(entry));
      }
    }

    // 2. Inform application
    if (impl_->user_error_callback_ &&
        info.level <= config_.runtime.logging.level) {
      impl_->user_error_callback_(info);
    }
  });

  impl_->device_scanner_ = std::make_unique<detail::DeviceScanner>(
      config_.runtime.address, impl_->device_manager_.get());

  impl_->poll_manager_ = std::make_unique<detail::PollManager>();

  impl_->bus_handler_ = std::make_unique<detail::BusHandler>(
      impl_->request_.get(), impl_->handler_.get(), impl_->bus_->getQueue(),
      defaults::Bus::detail::event_queue_capacity);

  impl_->client_manager_ = std::make_unique<detail::ClientManager>(
      impl_->bus_.get(), impl_->bus_handler_.get(), impl_->request_.get(),
      impl_->bus_monitor_.get());
  impl_->client_manager_->setActiveTimeout(
      config_.runtime.network.timing.client_timeout);
  impl_->client_manager_->setOutboundBufferSize(
      config_.runtime.network.outbound_buffer_size);

  impl_->bus_->setWindow(config_.runtime.bus.timing.window);
  impl_->bus_->setOffset(config_.runtime.bus.timing.offset);
}

void Controller::run() {
  while (running_) {
    bool activity = false;
    impl_->poll_manager_->processDueItems([this](const detail::PollItem& item) {
      impl_->scheduler_->enqueue(item.priority, item.message, item.callback);
    });

    if (impl_->scheduler_->queueSize() < defaults::Scheduler::scan_threshold) {
      auto scan_cmd = impl_->device_scanner_->nextCommand();
      if (!scan_cmd.empty()) {
        impl_->scheduler_->enqueue(defaults::detail::Scanner::scan_priority,
                                   scan_cmd);
        activity = true;
      }
    }

    std::unique_lock<std::mutex> lk(wake_mutex_);
    wake_cv_.wait_for(
        lk, std::chrono::milliseconds(defaults::Scheduler::controller_tick_ms),
        [this, activity] { return !running_ || activity; });
  }
}

}  // namespace ebus
