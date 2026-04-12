/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/device.hpp>

#include "app/client_manager.hpp"
#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/service_thread.hpp"
#include "platform/system.hpp"
#include "utils/common.hpp"

struct ebus::Impl {
  ebus::TelegramCallback userTelegramCallback;
  ebus::ErrorCallback userErrorCallback;
  std::unique_ptr<ebus::Request> request;
  std::unique_ptr<ebus::Bus> bus;
  std::unique_ptr<ebus::BusHandler> busHandler;
  std::unique_ptr<ebus::Handler> handler;
  std::unique_ptr<ebus::DeviceManager> deviceManager;
  std::unique_ptr<ebus::DeviceScanner> deviceScanner;
  std::unique_ptr<ebus::PollManager> pollManager;
  std::unique_ptr<ebus::Scheduler> scheduler;
  std::unique_ptr<ebus::ClientManager> clientManager;
  std::unique_ptr<ebus::ServiceThread> worker;
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
  impl_->bus->start();
  impl_->busHandler->start();
  impl_->scheduler->start();
  impl_->clientManager->start();
  impl_->worker = std::make_unique<ServiceThread>(
      "ebusController", [this] { run(); }, 4096, 1, 0);
  impl_->worker->start();
}

void ebus::Controller::stop() {
  if (!configured_) return;
  if (!running_) return;
  running_ = false;
  if (impl_->worker) impl_->worker->join();
  impl_->clientManager->stop();
  impl_->scheduler->stop();
  impl_->busHandler->stop();
  impl_->bus->stop();
}

void ebus::Controller::setAddress(const uint8_t& address) {
  config_.runtime.address = address;
  if (configured_) {
    impl_->handler->setSourceAddress(address);
    impl_->deviceManager->setOwnAddress(address);
    impl_->deviceScanner->setOwnAddress(address);
    impl_->bus->setRuntimeConfig(config_.runtime);
    impl_->request->setMaxLockCounter(config_.runtime.lock_counter_max);
  }
}

void ebus::Controller::setWindow(const uint16_t& window) {
  config_.runtime.window = window;
  if (configured_) impl_->bus->setWindow(window);
}

void ebus::Controller::setOffset(const uint16_t& offset) {
  config_.runtime.offset = offset;
  if (configured_) impl_->bus->setOffset(offset);
}

void ebus::Controller::setClientActiveTimeout(
    std::chrono::milliseconds timeout) {
  if (impl_->clientManager) impl_->clientManager->setActiveTimeout(timeout);
}

void ebus::Controller::setTelegramCallback(TelegramCallback callback) {
  impl_->userTelegramCallback = std::move(callback);
}

void ebus::Controller::setErrorCallback(ErrorCallback callback) {
  impl_->userErrorCallback = std::move(callback);
}

void ebus::Controller::enqueue(
    uint8_t priority, const std::vector<uint8_t>& message,
    std::function<void(bool success, const std::vector<uint8_t>& master,
                       const std::vector<uint8_t>& slave)>
        callback) {
  if (impl_->scheduler)
    impl_->scheduler->enqueue(priority, message, std::move(callback));
}

uint32_t ebus::Controller::addPollItem(
    uint8_t priority, const std::vector<uint8_t>& message,
    std::chrono::seconds interval,
    std::function<void(const std::vector<uint8_t>&)> callback) {
  return impl_->pollManager
             ? impl_->pollManager->addPollItem(priority, message, interval,
                                               std::move(callback))
             : 0;
}

void ebus::Controller::removePollItem(uint32_t id) {
  if (impl_->pollManager) impl_->pollManager->removePollItem(id);
}

void ebus::Controller::setFullScan(bool enable) {
  if (impl_->deviceScanner) impl_->deviceScanner->setFullScan(enable);
}

void ebus::Controller::setScanOnStartup(bool enable) {
  if (impl_->deviceScanner) impl_->deviceScanner->setScanOnStartup(enable);
}

void ebus::Controller::scanAddress(uint8_t address) {
  if (impl_->deviceScanner) impl_->deviceScanner->scanAddress(address);
}

void ebus::Controller::scanObservedDevices() {
  if (impl_->deviceScanner) impl_->deviceScanner->scanObservedDevices();
}

bool ebus::Controller::isScanning() const {
  return impl_->deviceScanner ? impl_->deviceScanner->isScanning() : false;
}

void ebus::Controller::addClient(int fd, ClientType type) {
  if (impl_->clientManager) impl_->clientManager->addClient(fd, type);
}

void ebus::Controller::removeClient(int fd) {
  if (impl_->clientManager) impl_->clientManager->removeClient(fd);
}

std::vector<ebus::DeviceInfo> ebus::Controller::getDeviceInfo() const {
  return impl_->deviceManager ? impl_->deviceManager->getDeviceInfo()
                              : std::vector<DeviceInfo>();
}

std::map<std::string, ebus::MetricValues> ebus::Controller::getMetrics() const {
  if (!configured_) return {};

  std::map<std::string, MetricValues> metrics;
  auto hMetrics = impl_->handler->getMetrics();
  metrics.insert(hMetrics.begin(), hMetrics.end());

  auto rMetrics = impl_->request->getMetrics();
  metrics.insert(rMetrics.begin(), rMetrics.end());

  auto bMetrics = impl_->bus->getMetrics();
  metrics.insert(bMetrics.begin(), bMetrics.end());

  // 4. Calculate Aggregate Bus Quality (%)
  // Quality combines Protocol Health (Error Rate) and Network Congestion
  // (Contention Rate)
  double errorRate = metrics.count("handler.errorRate")
                         ? metrics["handler.errorRate"].last
                         : 0.0;
  double contentionRate = metrics.count("request.contentionRate")
                              ? metrics["request.contentionRate"].last
                              : 0.0;

  if (metrics.count("handler.counter.messagesTotal") &&
      metrics["handler.counter.messagesTotal"].last > 0) {
    // Score is (100 - ErrorRate) * (1 - ContentionRate/100)
    // This means 100% error or 100% contention results in 0 quality.
    double quality = (100.0 - errorRate) * (1.0 - (contentionRate / 100.0));
    if (quality < 0) quality = 0;  // Ensure non-negative
    metrics["bus.quality"] = {quality, quality, quality, quality, 0.0, 1};
  } else {
    // If no messages, assume perfect quality (or no data to judge)
    metrics["bus.quality"] = {100.0, 100.0, 100.0, 100.0, 0.0, 0};
  }

  return metrics;
}

bool ebus::Controller::isConfigured() const noexcept { return configured_; }

bool ebus::Controller::isRunning() const noexcept { return running_; }

void ebus::Controller::constructMembers() {
  impl_->request.reset(new Request());
  impl_->request->setMaxLockCounter(config_.runtime.lock_counter_max);
  impl_->bus.reset(new Bus(config_.bus, config_.runtime, impl_->request.get()));
  impl_->handler.reset(new Handler(config_.runtime.address, impl_->bus.get(),
                                   impl_->request.get()));

  impl_->scheduler.reset(new Scheduler(impl_->handler.get()));

  impl_->deviceManager.reset(new DeviceManager());
  impl_->deviceManager->setOwnAddress(config_.runtime.address);

  // Setup the central dispatcher via the Scheduler.
  // The Scheduler handles the timing-critical Handler interaction and
  // provides an 'extern' callback hook which we use to feed the rest of the
  // app.
  impl_->scheduler->setTelegramCallback(
      [this](const MessageType& mType, const TelegramType& tType,
             const std::vector<uint8_t>& master,
             const std::vector<uint8_t>& slave) {
        // 1. Update Internal State (Device Discovery)
        if (impl_->deviceManager) {
          impl_->deviceManager->update(master, slave);
        }
        // 2. Inform the Application (Active, Passive, and Reactive)
        if (impl_->userTelegramCallback) {
          impl_->userTelegramCallback(mType, tType, master, slave);
        }
      });

  impl_->scheduler->setErrorCallback([this](const std::string& error,
                                            const std::vector<uint8_t>& master,
                                            const std::vector<uint8_t>& slave) {
    if (impl_->userErrorCallback) {
      impl_->userErrorCallback(error, master, slave);
    }
  });

  impl_->deviceScanner.reset(
      new DeviceScanner(config_.runtime.address, impl_->deviceManager.get()));

  impl_->pollManager.reset(new PollManager());

  impl_->busHandler.reset(new BusHandler(
      impl_->request.get(), impl_->handler.get(), impl_->bus->getQueue()));

  impl_->clientManager.reset(new ClientManager(
      impl_->bus.get(), impl_->busHandler.get(), impl_->request.get()));
  impl_->clientManager->setActiveTimeout(config_.client_timeout_ms);

  impl_->bus->setWindow(config_.runtime.window);
  impl_->bus->setOffset(config_.runtime.offset);
}

void ebus::Controller::run() {
  while (running_) {
    auto dueItems = impl_->pollManager->getDueItems();
    for (const auto& item : dueItems) {
      impl_->scheduler->enqueue(item.priority, item.message);
    }

    if (impl_->scheduler->queueSize() < 5) {
      auto scanCmd = impl_->deviceScanner->nextCommand();
      if (!scanCmd.empty()) {
        impl_->scheduler->enqueue(5, scanCmd);
      }
    }

    // High-level "Tick" rate (100ms is enough for register polling resolution)
    ebus::sleepMs(100);
  }
}
