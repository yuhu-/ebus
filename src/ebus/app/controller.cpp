/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/device.hpp>
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

struct ebus::Impl {
  ebus::ReactiveMasterSlaveCallback user_reactive_callback_;
  ebus::TelegramCallback user_telegram_callback_;
  ebus::ErrorCallback user_error_callback_;
  std::unique_ptr<ebus::Request> request_;
  std::unique_ptr<ebus::BusMonitor> bus_monitor_;
  std::unique_ptr<ebus::Bus> bus_;
  std::unique_ptr<ebus::BusHandler> bus_handler_;
  std::unique_ptr<ebus::Handler> handler_;
  std::unique_ptr<ebus::DeviceManager> device_manager_;
  std::unique_ptr<ebus::DeviceScanner> device_scanner_;
  std::unique_ptr<ebus::PollManager> poll_manager_;
  std::unique_ptr<ebus::Scheduler> scheduler_;
  std::unique_ptr<ebus::ClientManager> client_manager_;
  std::unique_ptr<ebus::ServiceThread> worker_;
};

ebus::Controller::Controller(const EbusConfig& config)
    : config_(config), impl_(new Impl()) {
  configured_ = true;
  constructMembers();
}

ebus::Controller::~Controller() { stop(); }

void ebus::Controller::configure(const EbusConfig& config) {
  if (running_) return;
  config_ = config;
  configured_ = true;
  constructMembers();
}

void ebus::Controller::start() {
  if (!configured_) return;
  if (running_) return;
  running_ = true;
  impl_->bus_->start();
  impl_->bus_handler_->start();
  impl_->scheduler_->start();
  impl_->client_manager_->start();
  impl_->worker_ = std::make_unique<ServiceThread>(
      "ebusController", [this] { run(); }, 4096, 1, 0);
  impl_->worker_->start();
}

void ebus::Controller::stop() {
  if (!configured_) return;
  if (!running_) return;
  running_ = false;
  if (impl_->worker_) impl_->worker_->join();
  impl_->client_manager_->stop();
  impl_->scheduler_->stop();
  impl_->bus_handler_->stop();
  impl_->bus_->stop();
}

void ebus::Controller::setAddress(const uint8_t& address) {
  config_.runtime.address = address;
  if (configured_) {
    impl_->handler_->setSourceAddress(address);
    impl_->device_manager_->setOwnAddress(address);
    impl_->device_scanner_->setOwnAddress(address);
    impl_->bus_->setRuntimeConfig(config_.runtime);
    impl_->request_->setMaxLockCounter(config_.runtime.lock_counter_max);
  }
}

void ebus::Controller::setWindow(const uint16_t& window) {
  config_.runtime.window = window;
  if (configured_) impl_->bus_->setWindow(window);
}

void ebus::Controller::setOffset(const uint16_t& offset) {
  config_.runtime.offset = offset;
  if (configured_) impl_->bus_->setOffset(offset);
}

void ebus::Controller::setClientActiveTimeout(
    std::chrono::milliseconds timeout) {
  if (impl_->client_manager_) impl_->client_manager_->setActiveTimeout(timeout);
}

void ebus::Controller::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  impl_->user_reactive_callback_ = std::move(callback);
  if (impl_->scheduler_) {
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
  }
}

void ebus::Controller::setTelegramCallback(TelegramCallback callback) {
  impl_->user_telegram_callback_ = std::move(callback);
}

void ebus::Controller::setErrorCallback(ErrorCallback callback) {
  impl_->user_error_callback_ = std::move(callback);
}

void ebus::Controller::enqueue(uint8_t priority, ByteView message,
                               ResultCallback callback) {
  if (impl_->scheduler_)
    impl_->scheduler_->enqueue(priority, message, std::move(callback));
}

uint32_t ebus::Controller::addPollItem(uint8_t priority, ByteView message,
                                       std::chrono::milliseconds interval,
                                       std::function<void(ByteView)> callback) {
  return impl_->poll_manager_
             ? impl_->poll_manager_->addPollItem(priority, message, interval,
                                                 std::move(callback))
             : 0;
}

void ebus::Controller::removePollItem(uint32_t id) {
  if (impl_->poll_manager_) impl_->poll_manager_->removePollItem(id);
}

void ebus::Controller::setFullScan(bool enable) {
  if (impl_->device_scanner_) impl_->device_scanner_->setFullScan(enable);
}

void ebus::Controller::setScanOnStartup(bool enable) {
  if (impl_->device_scanner_) impl_->device_scanner_->setScanOnStartup(enable);
}

void ebus::Controller::scanAddress(uint8_t address) {
  if (impl_->device_scanner_) impl_->device_scanner_->scanAddress(address);
}

void ebus::Controller::scanObservedDevices() {
  if (impl_->device_scanner_) impl_->device_scanner_->scanObservedDevices();
}

bool ebus::Controller::isScanning() const {
  return impl_->device_scanner_ ? impl_->device_scanner_->isScanning() : false;
}

void ebus::Controller::addClient(int fd, ClientType type) {
  if (impl_->client_manager_) impl_->client_manager_->addClient(fd, type);
}

void ebus::Controller::removeClient(int fd) {
  if (impl_->client_manager_) impl_->client_manager_->removeClient(fd);
}

std::vector<ebus::DeviceInfo> ebus::Controller::getDeviceInfo() const {
  return impl_->device_manager_ ? impl_->device_manager_->getDeviceInfo()
                                : std::vector<DeviceInfo>();
}

ebus::Metrics ebus::Controller::getMetrics() const {
  if (!configured_) return {};

  metrics::SystemMetrics sm;
  if (impl_->bus_monitor_) {
    sm.handler = impl_->bus_monitor_->getHandlerMetrics();
    sm.request = impl_->bus_monitor_->getRequestMetrics();
    sm.bus = impl_->bus_monitor_->getBusMetrics();

    // Calculate Quality Score (%)
    sm.quality = (100.0 - sm.handler.error_rate) *
                 (1.0 - (sm.request.contention_rate / 100.0));
  }

  return sm;
}

bool ebus::Controller::isConfigured() const noexcept { return configured_; }

bool ebus::Controller::isRunning() const noexcept { return running_; }

void ebus::Controller::constructMembers() {
  impl_->bus_monitor_.reset(new BusMonitor());
  impl_->request_.reset(new Request(impl_->bus_monitor_.get()));
  impl_->request_->setMaxLockCounter(config_.runtime.lock_counter_max);
  impl_->bus_.reset(new Bus(config_.bus, config_.runtime, impl_->request_.get(),
                            impl_->bus_monitor_.get()));
  impl_->handler_.reset(new Handler(config_.runtime.address, impl_->bus_.get(),
                                    impl_->request_.get(),
                                    impl_->bus_monitor_.get()));

  impl_->scheduler_.reset(new Scheduler(impl_->handler_.get()));

  impl_->device_manager_.reset(new DeviceManager());
  impl_->device_manager_->setOwnAddress(config_.runtime.address);

  if (impl_->user_reactive_callback_) {
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
  }

  // Setup the central dispatcher via the Scheduler.
  // The Scheduler handles the timing-critical Handler interaction and
  // provides an 'extern' callback hook which we use to feed the rest of the
  // app.
  impl_->scheduler_->setTelegramCallback(
      [this](MessageType message_type, TelegramType telegram_type,
             ByteView master_view, ByteView slave_view) {
        // 1. Update Internal State (Device Discovery)
        if (impl_->device_manager_) {
          impl_->device_manager_->update(master_view, slave_view);
        }
        // 2. Inform the Application (Active, Passive, and Reactive)
        if (impl_->user_telegram_callback_) {
          impl_->user_telegram_callback_(message_type, telegram_type,
                                         master_view, slave_view);
        }
      });

  impl_->scheduler_->setErrorCallback([this](std::string_view error,
                                             ByteView master_view,
                                             ByteView slave_view) {
    if (impl_->user_error_callback_) {
      impl_->user_error_callback_(error, master_view, slave_view);
    }
  });

  impl_->device_scanner_.reset(
      new DeviceScanner(config_.runtime.address, impl_->device_manager_.get()));

  impl_->poll_manager_.reset(new PollManager());

  impl_->bus_handler_.reset(new BusHandler(
      impl_->request_.get(), impl_->handler_.get(), impl_->bus_->getQueue()));

  impl_->client_manager_.reset(
      new ClientManager(impl_->bus_.get(), impl_->bus_handler_.get(),
                        impl_->request_.get(), impl_->bus_monitor_.get()));
  impl_->client_manager_->setActiveTimeout(config_.client_timeout_ms);

  impl_->bus_->setWindow(config_.runtime.window);
  impl_->bus_->setOffset(config_.runtime.offset);
}

void ebus::Controller::run() {
  while (running_) {
    impl_->poll_manager_->processDueItems([this](const PollItem& item) {
      impl_->scheduler_->enqueue(item.priority, item.message);
    });

    if (impl_->scheduler_->queueSize() < 5) {
      auto scan_cmd = impl_->device_scanner_->nextCommand();
      if (!scan_cmd.empty()) {
        impl_->scheduler_->enqueue(5, scan_cmd);
      }
    }

    // High-level "Tick" rate (100ms is enough for register polling resolution)
    ebus::sleepMs(100);
  }
}
