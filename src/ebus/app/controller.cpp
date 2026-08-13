/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>

#if EBUS_SIMULATION
#include <ebus/virtual_bus.hpp>
#endif
#include <algorithm>
#include <chrono>
#include <memory>

#include "app/client_manager.hpp"
#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/reactor.hpp"
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "utils/circular_buffer.hpp"
#include "utils/logger.hpp"

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

namespace ebus {

struct Impl {
  ebus::ReactiveCallback user_reactive_callback_;
  ebus::ProtocolCallback user_protocol_callback_;
  ebus::TraceCallback user_trace_callback_;

  std::atomic<bool> configured_{false};
  std::atomic<bool> running_{false};

  std::unique_ptr<detail::Request> request_;
  std::unique_ptr<detail::BusMonitor> bus_monitor_;
  std::unique_ptr<detail::platform::Bus> bus_;
  std::unique_ptr<detail::BusHandler> bus_handler_;
  std::unique_ptr<detail::Handler> handler_;
  std::unique_ptr<detail::DeviceManager> device_manager_;
  std::unique_ptr<detail::DeviceScanner> device_scanner_;
  std::unique_ptr<detail::PollManager> poll_manager_;
  std::unique_ptr<detail::Scheduler> scheduler_;
  std::unique_ptr<detail::Reactor> reactor_;
#if EBUS_SIMULATION
  std::unique_ptr<ebus::VirtualBus> virtual_bus_;
#endif
  std::unique_ptr<detail::ClientManager> client_manager_;

  std::atomic<LogLevel> log_level_{LogLevel::error};
  std::atomic<uint8_t> address_{0xff};

  mutable detail::platform::RecursiveMutex
      config_mutex_;  // Protects config_ and related members

  void fetchServiceStatus(ServiceStatus& status) const;

  void constructMembers(Controller* owner);

  // Predicates for resource fairness
  bool isSchedulerFull() const;
  bool isHandlerBusy() const;
  bool isSystemBusy() const;
};

Controller::Controller() : impl_(new Impl()) {}

Controller::Controller(const EbusConfig& config)
    : config_(config), impl_(new Impl()) {
  impl_->constructMembers(this);
  impl_->configured_.store(true);
}

Controller::~Controller() { stop(); }

bool Controller::start() {
  if (!impl_->configured_.load()) return false;
  bool expected = false;
  if (!impl_->running_.compare_exchange_strong(expected, true)) return true;

  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);

  assert(impl_->bus_ && impl_->client_manager_ && impl_->reactor_ &&
         "All subsystems must be initialized before start()");

  impl_->bus_->start();
  impl_->client_manager_->start(config_.runtime);

  impl_->reactor_->setProtocolCallback(impl_->user_protocol_callback_);
  impl_->reactor_->setTraceCallback(impl_->user_trace_callback_);
  impl_->reactor_->setLogLevel(impl_->log_level_.load());

  impl_->reactor_->start();

  if (config_.runtime.system_inquiry) triggerInquiryOfExistence();

  return true;
}

void Controller::stop() {
  EBUS_LOG_INFO_F("[0x%02x] Stopping controller services.",
                  impl_->address_.load(std::memory_order_relaxed));
  bool expected = true;
  if (!impl_->running_.compare_exchange_strong(expected, false)) return;

  if (impl_->reactor_) {
    impl_->reactor_->stop();
  }

  impl_->client_manager_->stop();
  impl_->scheduler_->stop();
  impl_->bus_->stop();
}

bool Controller::configure(const EbusConfig& config) {
  // 1. Exhaustive Validation
  if (!detail::ConfigValidator::validate(config)) return false;

  // 2. Restart-Check: Some BusConfig changes require a full stop
  if (impl_->running_.load()) {
    if (detail::ConfigValidator::requiresHardwareRestart(config_, config)) {
      return false;  // Cannot change physical UART while running
    }
  }

  // 3. Apply changes
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_ = config;
  impl_->constructMembers(this);
  impl_->configured_.store(true);
  return true;
}

bool Controller::configure(std::string_view json) {
  if (!detail::ConfigValidator::validateJson(json)) return false;

  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  EbusConfig new_cfg = config_;
  // mergeFromJson safely ignores unknown keys.
  if (!new_cfg.runtime.mergeFromJson(json)) return false;

  return configure(new_cfg);
}

EbusConfig Controller::getConfig() const {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  return config_;
}

void Controller::setAddress(const uint8_t& address) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.address = address;
  impl_->address_.store(address,
                        std::memory_order_relaxed);  // Update internal atomic
  if (impl_->configured_.load()) {
    impl_->handler_->setSourceAddress(address);
    impl_->handler_->reset();
    impl_->request_->reset();
    impl_->device_manager_->setOwnAddress(address);
    impl_->device_scanner_->setOwnAddress(address);
    impl_->poll_manager_->setOwnAddress(address);
    impl_->bus_->setRuntimeConfig(config_.runtime);
    impl_->request_->setLockCounter(config_.runtime.lock_counter);
  }
}

void Controller::setLockCounter(const uint8_t& lock_counter) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.lock_counter = lock_counter;
  if (impl_->configured_.load()) {
    impl_->request_->setLockCounter(lock_counter);
  }
}

void Controller::setSystemInquiry(bool enable) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.system_inquiry = enable;
}

void Controller::setSystemResponse(bool enable) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.system_response = enable;
}

void Controller::setWindow(const uint16_t& window_us) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.bus.window_us = window_us;
  if (impl_->configured_.load()) impl_->bus_->setWindow(window_us);
}

void Controller::setOffset(const uint16_t& offset_us) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.bus.offset_us = offset_us;
  if (impl_->configured_.load()) impl_->bus_->setOffset(offset_us);
}

void Controller::setWatchdogTimeout(uint32_t timeout_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.bus.watchdog_timeout_ms = timeout_ms;
  if (impl_->configured_.load()) {
    impl_->bus_handler_->setWatchdogTimeout(timeout_ms);
  }
}

void Controller::setLogLevel(LogLevel level) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.log_level = level;
  impl_->log_level_.store(level, std::memory_order_relaxed);
  auto current = detail::Logger::getInstance().getLevel();
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(current)) {
    detail::Logger::getInstance().setLevel(level);
  }
}

void Controller::setLogSink(LogCallback sink) {
  detail::Logger::getInstance().setSink(std::move(sink));
}

void Controller::setSessionTimeout(uint32_t timeout_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.network.session_timeout_ms = timeout_ms;
  if (impl_->configured_.load())
    impl_->client_manager_->setSessionTimeout(timeout_ms);
}

void Controller::setTransmitTimeout(uint32_t timeout_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.network.transmit_timeout_ms = timeout_ms;
  if (impl_->configured_.load())
    impl_->client_manager_->setTransmitTimeout(timeout_ms);
}

void Controller::setOutgoingBufferSize(size_t size) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.network.outbound_buffer_size = size;
  if (impl_->configured_.load()) {
    impl_->client_manager_->setOutgoingBufferSize(size);
  }
}

void Controller::setScanOnStartup(bool enable) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.device.scan_on_startup = enable;
  if (impl_->configured_.load())
    impl_->device_scanner_->setScanOnStartup(enable);
}

void Controller::setMaxStartupScans(uint8_t max_scans) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.device.max_startup_scans = max_scans;
  if (impl_->configured_.load()) {
    impl_->device_scanner_->setMaxStartupScans(max_scans);
  }
}

void Controller::setInitialScanDelay(uint32_t delay_s) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.device.initial_delay_s = delay_s;
  if (impl_->configured_.load()) {
    impl_->device_scanner_->setInitialScanDelay(delay_s);
  }
}

void Controller::setStartupScanInterval(uint32_t interval_s) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.device.startup_interval_s = interval_s;
  if (impl_->configured_.load()) {
    impl_->device_scanner_->setStartupScanInterval(interval_s);
  }
}

void Controller::setMaxSendAttempts(uint8_t max_send_attempts) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.scheduler.max_send_attempts = max_send_attempts;
  if (impl_->configured_.load()) {
    impl_->scheduler_->setMaxSendAttempts(max_send_attempts);
  }
}

void Controller::setBaseBackoff(uint32_t base_backoff_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.scheduler.base_backoff_ms = base_backoff_ms;
  if (impl_->configured_.load())
    impl_->scheduler_->setBaseBackoff(base_backoff_ms);
}

void Controller::setFsmTimeout(uint32_t timeout_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.scheduler.fsm_timeout_ms = timeout_ms;
  if (impl_->configured_.load()) {
    impl_->scheduler_->setFsmTimeout(timeout_ms);
  }
}

void Controller::setTotalTimeout(uint32_t timeout_ms) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.scheduler.total_timeout_ms = timeout_ms;
  if (impl_->configured_.load()) impl_->scheduler_->setTotalTimeout(timeout_ms);
}

void Controller::setReactiveCallback(ReactiveCallback callback) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->user_reactive_callback_ = std::move(callback);
  if (impl_->configured_.load())
    impl_->scheduler_->setReactiveCallback(impl_->user_reactive_callback_);
}

void Controller::setProtocolCallback(ProtocolCallback callback) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->user_protocol_callback_ = std::move(callback);
  if (impl_->reactor_) {
    impl_->reactor_->setProtocolCallback(impl_->user_protocol_callback_);
  }
}

void Controller::setTraceCallback(TraceCallback callback) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->user_trace_callback_ = std::move(callback);
  if (impl_->reactor_) {
    impl_->reactor_->setTraceCallback(impl_->user_trace_callback_);
  }
}

uint32_t Controller::enqueue(uint8_t priority, ByteView message) {
  if (!impl_->configured_.load()) return 0;
  uint32_t s_id = impl_->scheduler_->enqueue(priority, message);
  if (s_id > 0 && impl_->reactor_) {
    detail::ReactorSignal ev;
    ev.type = detail::ReactorSignal::Type::user_request;
    impl_->reactor_->pushSignal(std::move(ev));
  }
  return s_id;
}

uint32_t Controller::enqueueAt(uint8_t priority, ByteView message,
                               Clock::time_point when) {
  if (!impl_->configured_.load()) return 0;
  uint32_t s_id = impl_->scheduler_->enqueueAt(priority, message, when);
  if (s_id > 0 && impl_->reactor_) {
    detail::ReactorSignal ev;
    ev.type = detail::ReactorSignal::Type::user_request;
    impl_->reactor_->pushSignal(std::move(ev));
  }
  return s_id;
}

uint32_t Controller::addPollItem(uint8_t priority, ByteView message,
                                 uint32_t interval_ms) {
  uint32_t id = impl_->configured_.load() ? impl_->poll_manager_->addPollItem(
                                                priority, message, interval_ms)
                                          : 0;
  if (id != 0 && impl_->reactor_) {
    detail::ReactorSignal ev;
    ev.type = detail::ReactorSignal::Type::timer_wakeup;
    impl_->reactor_->pushSignal(std::move(ev));
  }
  return id;
}

void Controller::removePollItem(uint32_t id) {
  if (impl_->configured_.load()) impl_->poll_manager_->removePollItem(id);
}

void Controller::clearPollItems() {
  if (impl_->configured_.load()) impl_->poll_manager_->clear();
}

void Controller::triggerInquiryOfExistence() {
  enqueue(detail::DeviceLimits::scan_priority,
          ebus::Sequence::inquiryOfExistence());
}

void Controller::initFullScan(bool enable) {
  if (impl_->configured_.load()) impl_->device_scanner_->initFullScan(enable);
}

bool Controller::scanAddress(uint8_t address) {
  if (impl_->configured_.load()) {
    if (impl_->device_scanner_->scanAddress(address)) {
      if (impl_->reactor_) {
        detail::ReactorSignal ev;
        ev.type = detail::ReactorSignal::Type::timer_wakeup;
        impl_->reactor_->pushSignal(std::move(ev));
      }
      return true;
    }
  }
  return false;
}

bool Controller::scanAddresses(const std::vector<uint8_t>& addresses) {
  if (impl_->configured_.load()) {
    if (impl_->device_scanner_->scanAddresses(addresses)) {
      if (impl_->reactor_) {
        detail::ReactorSignal ev;
        ev.type = detail::ReactorSignal::Type::timer_wakeup;
        impl_->reactor_->pushSignal(std::move(ev));
      }
      return true;
    }
  }
  return false;
}

bool Controller::scanObservedDevices() {
  return impl_->configured_.load()
             ? impl_->device_scanner_->scanObservedDevices()
             : false;
}

bool Controller::addClient(int fd, ClientType type) {
  return impl_->configured_.load() ? impl_->client_manager_->addClient(fd, type)
                                   : false;
}

void Controller::removeClient(int fd) {
  if (impl_->configured_.load()) impl_->client_manager_->removeClient(fd);
}

bool Controller::isRunning() const noexcept { return impl_->running_.load(); }

bool Controller::isConfigured() const noexcept {
  return impl_->configured_.load();
}

bool Controller::isScanning() const {
  return impl_->configured_.load() ? impl_->device_scanner_->isScanning()
                                   : false;
}

void Controller::fetchDevices(
    std::function<void(const DeviceInfo&)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->device_manager_->fetchDevices(callback);
  }
}

void Controller::fetchDevices(const JsonChunkVisitor& visitor,
                              bool pretty) const {
  if (impl_->configured_.load() && visitor) {
    detail::JsonWriter writer(visitor, pretty);
    auto scope = writer.arrayScope();
    impl_->device_manager_->fetchDevices(
        [&](const DeviceInfo& d) { writer.writeValue(d); });
  }
}

void Controller::fetchMetrics(
    std::function<void(const Metrics&)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->bus_monitor_->fetchMetrics(callback);
  }
}

void Controller::fetchMetrics(const JsonChunkVisitor& visitor,
                              bool pretty) const {
  if (impl_->configured_.load() && visitor) {
    impl_->bus_monitor_->fetchMetrics([&](const Metrics& m) {
      detail::JsonWriter writer(visitor, pretty);
      m.toJson(writer);
    });
  }
}

void Controller::fetchUtilizationHistory(
    std::function<void(float)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->bus_monitor_->fetchUtilizationHistory(callback);
  }
}

void Controller::fetchUtilizationHistory(const JsonChunkVisitor& visitor,
                                         bool pretty) const {
  if (impl_->configured_.load() && visitor) {
    detail::JsonWriter writer(visitor, pretty);
    auto scope = writer.arrayScope();
    impl_->bus_monitor_->fetchUtilizationHistory(
        [&](float val) { writer.writeValueFloat(val); });
  }
}

void Controller::fetchTraceHistory(
    std::function<void(const BusEventInfo&)> callback) const {
  if (callback) impl_->reactor_->fetchTraceHistory(callback);
}

void Controller::fetchTraceHistory(const JsonChunkVisitor& visitor,
                                   bool pretty) const {
  if (visitor) impl_->reactor_->fetchTraceHistory(visitor, pretty);
}

void Controller::fetchErrors(
    std::function<void(const ErrorEntry&)> callback) const {
  if (callback) impl_->reactor_->fetchErrors(callback);
}

void Controller::fetchErrors(const JsonChunkVisitor& visitor,
                             bool pretty) const {
  if (visitor) impl_->reactor_->fetchErrors(visitor, pretty);
}

size_t Controller::getErrorLogCapacity() const {
  return impl_->reactor_->getErrorLogCapacity();
}

void Controller::fetchStatus(
    std::function<void(const SystemResources&)> callback) const {
  if (!callback) return;

  SystemResources res;
  res.is_configured = impl_->configured_.load();
  res.is_running = impl_->running_.load();

  if (impl_->configured_.load()) {
    ServiceStatus snapshot;
    impl_->fetchServiceStatus(snapshot);

    res.last_update_timestamp_ms = snapshot.last_update_timestamp_ms;

    if (!snapshot.reactor.thread.name.empty())
      res.threads.push_back(snapshot.reactor.thread);
    if (!snapshot.bus.bus_thread.name.empty())
      res.threads.push_back(snapshot.bus.bus_thread);
    if (!snapshot.bus.syn_thread.name.empty())
      res.threads.push_back(snapshot.bus.syn_thread);
    if (!snapshot.client_manager.thread.name.empty())
      res.threads.push_back(snapshot.client_manager.thread);

    if (snapshot.scheduler.queue.capacity > 0)
      res.queues.push_back(snapshot.scheduler.queue);
  }

  if (impl_->reactor_) {
    const auto reactor_status = impl_->reactor_->fetchStatus();
    res.queues.push_back(reactor_status.signal_queue);
    res.queues.push_back(reactor_status.protocol_queue);
    res.queues.push_back(reactor_status.bus_queue);
  }

  callback(res);
}

void Controller::fetchStatus(const JsonChunkVisitor& visitor,
                             bool pretty) const {
  ServiceStatus snapshot;
  if (impl_->configured_.load()) {
    impl_->fetchServiceStatus(snapshot);
  }
  serializeServiceStatus(visitor, snapshot, impl_->bus_monitor_.get(), pretty);
}

void Controller::clearHistories() {
  if (impl_->configured_.load()) impl_->bus_monitor_->clearHistory();
}

void Controller::resetMetrics() {
  if (impl_->configured_.load()) impl_->bus_monitor_->resetMetrics();
}

void Controller::clearErrors() { impl_->reactor_->clearErrors(); }

#if EBUS_SIMULATION
VirtualBus& Controller::getVirtualBus() { return *impl_->virtual_bus_; }
#endif

void Impl::fetchServiceStatus(ServiceStatus& status) const {
  status.last_update_timestamp_ms = ebus::getWallTimeMs();

  if (configured_.load()) {
    status.reactor = reactor_->fetchStatus();
    status.bus = bus_->fetchStatus();
    status.bus_handler = bus_handler_->fetchStatus();
    status.scheduler = scheduler_->fetchStatus();
    status.client_manager = client_manager_->fetchStatus();
    status.device_manager = device_manager_->fetchStatus();
    status.device_scanner = device_scanner_->fetchStatus();
    status.poll_manager = poll_manager_->fetchStatus();
  }
}

void Impl::constructMembers(Controller* owner) {
  // -- 1. Telemetry & Core Arbitration --
  if (!bus_monitor_) {
    bus_monitor_ = std::make_unique<detail::BusMonitor>();
  }

  if (!request_) {
    request_ = std::make_unique<detail::Request>(bus_monitor_.get());
  }

  // -- 2. Physical Layer --
  // Note: configure() ensures we don't change hardware params while running
  if (!bus_) {
    bus_ = std::make_unique<detail::platform::Bus>(
        owner->config_.bus, owner->config_.runtime, request_.get(),
        bus_monitor_.get());
  }

#if EBUS_SIMULATION
  // Initialize VirtualBus build flag is set
  if (!virtual_bus_) {
    virtual_bus_ =
        std::unique_ptr<ebus::VirtualBus>(new ebus::VirtualBus(*bus_));
  }
#endif

  // -- 3. Protocol Handler --
  if (!handler_) {
    handler_ = std::make_unique<detail::Handler>(owner->config_.runtime.address,
                                                 bus_.get(), request_.get(),
                                                 bus_monitor_.get());
  }

  // -- 4. Scheduler --
  if (!scheduler_) {
    scheduler_ = std::make_unique<detail::Scheduler>(handler_.get());
    scheduler_->attachHandlerCallbacks();
  }

  // -- 5. Application Logic --
  if (!device_manager_) {
    device_manager_ =
        std::make_unique<detail::DeviceManager>(bus_monitor_.get());
  }

  if (!device_scanner_) {
    device_scanner_ = std::make_unique<detail::DeviceScanner>(
        owner->config_.runtime.address, device_manager_.get());
  }

  if (!poll_manager_) {
    poll_manager_ = std::make_unique<detail::PollManager>();
    poll_manager_->setBusyPredicate(
        detail::Delegate<bool()>::bind<Impl, &Impl::isSchedulerFull>(this));
  }

  // -- 6. Plumbing --
  if (!bus_handler_) {
    bus_handler_ =
        std::make_unique<detail::BusHandler>(request_.get(), handler_.get());

    // Bridge Physical Bus Events -> BusHandler
    bus_->addBusEventListener(
        detail::Delegate<void(const detail::BusEvent& event)>::bind<
            detail::BusHandler, &detail::BusHandler::onBusEvent>(
            bus_handler_.get()));
  }

  if (!client_manager_) {
    client_manager_ = std::make_unique<detail::ClientManager>(
        bus_.get(), bus_handler_.get(), request_.get(), bus_monitor_.get());
  }

  // -- 7. Reactor --
  if (!reactor_) {
    reactor_ = std::make_unique<detail::Reactor>(
        owner->config_.runtime.address, owner->config_.runtime.system_response,
        scheduler_.get(), poll_manager_.get(), device_scanner_.get(),
        device_manager_.get(), bus_monitor_.get());

    // Wire Scheduler -> Reactor
    scheduler_->setProtocolEventSink([this](detail::ProtocolEvent&& ev) {
      reactor_->pushProtocolEvent(std::move(ev));
    });

    // Wire BusHandler -> Reactor (trace events)
    bus_handler_->setReactorBusEventInfoCallback(
        detail::Delegate<void(const BusEventInfo&)>::bind<
            detail::Reactor, &detail::Reactor::onBusEventInfo>(reactor_.get()));
  }

  // Centralized synchronization using setters. Recursive mutex allows this
  // safely.
  owner->setLogLevel(owner->config_.runtime.log_level);
  owner->setAddress(owner->config_.runtime.address);
  owner->setLockCounter(owner->config_.runtime.lock_counter);
  owner->setWindow(owner->config_.runtime.bus.window_us);
  owner->setOffset(owner->config_.runtime.bus.offset_us);
  owner->setWatchdogTimeout(owner->config_.runtime.bus.watchdog_timeout_ms);
  owner->setSessionTimeout(owner->config_.runtime.network.session_timeout_ms);
  owner->setTransmitTimeout(owner->config_.runtime.network.transmit_timeout_ms);
  owner->setOutgoingBufferSize(
      owner->config_.runtime.network.outbound_buffer_size);
  owner->setScanOnStartup(owner->config_.runtime.device.scan_on_startup);
  owner->setMaxStartupScans(owner->config_.runtime.device.max_startup_scans);
  owner->setInitialScanDelay(owner->config_.runtime.device.initial_delay_s);
  owner->setStartupScanInterval(
      owner->config_.runtime.device.startup_interval_s);
  owner->setMaxSendAttempts(owner->config_.runtime.scheduler.max_send_attempts);
  owner->setBaseBackoff(owner->config_.runtime.scheduler.base_backoff_ms);
  owner->setFsmTimeout(owner->config_.runtime.scheduler.fsm_timeout_ms);
  owner->setTotalTimeout(owner->config_.runtime.scheduler.total_timeout_ms);
  if (user_reactive_callback_) {
    owner->setReactiveCallback(user_reactive_callback_);
  }

  if (device_scanner_) {
    device_scanner_->setBusyPredicate(
        detail::Delegate<bool()>::bind<Impl, &Impl::isSystemBusy>(this));
  }
}

bool Impl::isSchedulerFull() const {
  return scheduler_ && scheduler_->size() >= scheduler_->capacity();
}

bool Impl::isHandlerBusy() const {
  return handler_ && handler_->isActiveMessagePending();
}

bool Impl::isSystemBusy() const {
  return (handler_ && handler_->isActiveMessagePending()) ||
         (client_manager_ && client_manager_->isSessionActive());
}

}  // namespace ebus
