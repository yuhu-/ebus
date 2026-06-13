/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/detail/circular_buffer.hpp>
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
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "utils/logger.hpp"

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

namespace ebus {

struct Impl {
  detail::CircularBuffer<ebus::ErrorEntry,
                         detail::DiagnosticsLimits::log_history_size>
      error_buffer_;
  detail::CircularBuffer<ebus::BusEventInfo,
                         detail::DiagnosticsLimits::trace_history_size>
      trace_buffer_;

  ebus::ReactiveCallback user_reactive_callback_;
  ebus::ProtocolCallback user_protocol_callback_;
  ebus::TraceCallback user_trace_callback_;

  // Decoupled queues for user callbacks (Prioritized drainage)
  detail::platform::Queue<ebus::ProtocolEvent> protocol_events_{
      detail::ControllerLimits::event_queue_size};

  detail::platform::Queue<ebus::OrchestrationEvent> reactor_queue_{
      detail::ControllerLimits::reactor_queue_size};

  std::atomic<size_t> max_protocol_events_{0};
  std::atomic<size_t> max_reactor_queue_{0};

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
#if EBUS_SIMULATION
  std::unique_ptr<ebus::VirtualBus> virtual_bus_;
#endif
  std::unique_ptr<detail::ClientManager> client_manager_;
  std::unique_ptr<detail::platform::ServiceThread> worker_;

  mutable detail::platform::Mutex status_mutex_;
  std::atomic<LogLevel> log_level_{LogLevel::error};
  std::atomic<uint8_t> address_{0xff};
  ServiceStatus status_cache_;

  mutable detail::platform::RecursiveMutex
      config_mutex_;  // Protects config_ and related members

  void getServiceStatus(ServiceStatus& status) const;
  void processPublicEvents();

  void constructMembers(Controller* owner);
  void run(Controller* owner);

  void onSchedulerEvent(OrchestrationEvent&& oev);
  void onBusEventInfo(const BusEventInfo& info);
  void onBusEvent(const detail::BusEvent& bus_ev);

  // Predicates for resource fairness
  bool isSchedulerFull() const;
  bool isHandlerBusy() const;
  bool isSystemBusy() const;

  void logError(std::string_view msg) const;
  void logInfo(std::string_view msg) const;
  void logDebug(std::string_view msg) const;
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

  // Reset internal queues to clear any previous shutdown state or stale events.
  impl_->reactor_queue_.reset();
  impl_->protocol_events_.reset();

  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->bus_->start();
  impl_->client_manager_->start();

  // Trigger initial system discovery if enabled
  if (config_.runtime.system_inquiry) triggerInquiryOfExistence();

  impl_->worker_ = std::make_unique<detail::platform::ServiceThread>(
      "ebus_controller", [this] { impl_->run(this); },
      detail::OrchestrationLimits::controller_stack_size,
      detail::OrchestrationLimits::controller_priority);
  impl_->worker_->start();

  return true;
}

void Controller::stop() {
  impl_->logInfo("Stopping controller services.");
  bool expected = true;
  if (!impl_->running_.compare_exchange_strong(expected, false)) return;

  // Signal the worker thread to shut down and unblock its queue.
  OrchestrationEvent shutdown_ev;
  shutdown_ev.type = OrchestrationEventType::shutdown;
  // Attempt to push the shutdown event. If the queue is full, the shutdown()
  // call below will still ensure termination.
  impl_->reactor_queue_.tryPush(std::move(shutdown_ev));
  impl_->reactor_queue_.shutdown();

  if (impl_->worker_) impl_->worker_->join();
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
  config_.runtime.diagnostics.level = level;
  impl_->log_level_.store(level,
                          std::memory_order_relaxed);  // Update internal atomic
  // In multi-controller environments (sim), we keep the most verbose level
  // requested.
  auto current = detail::Logger::getInstance().getLevel();
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(current)) {
    detail::Logger::getInstance().setLevel(level);
  }
}

void Controller::setLogSink(LogCallback sink) {
  detail::Logger::getInstance().setSink(std::move(sink));
}

void Controller::setErrorLogSize(size_t size) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.diagnostics.log_size = size;
  if (impl_->configured_.load())
    impl_->error_buffer_.clear();  // Reset log on size change
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

void Controller::setOutboundBufferSize(size_t size) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  config_.runtime.network.outbound_buffer_size = size;
  if (impl_->configured_.load()) {
    impl_->client_manager_->setOutboundBufferSize(size);
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
    impl_->scheduler_->setReactiveCallback(
        impl_->user_reactive_callback_);
}

void Controller::setProtocolCallback(ProtocolCallback callback) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->user_protocol_callback_ = std::move(callback);
}

void Controller::setTraceCallback(TraceCallback callback) {
  detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
      impl_->config_mutex_);
  impl_->user_trace_callback_ = std::move(callback);
}

uint32_t Controller::enqueue(uint8_t priority, ByteView message) {
  if (!impl_->configured_.load()) return 0;
  uint32_t s_id = impl_->scheduler_->enqueue(priority, message);
  if (s_id > 0) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::user_request;
    impl_->reactor_queue_.tryPush(std::move(ev));
  }
  return s_id;
}

uint32_t Controller::enqueueAt(uint8_t priority, ByteView message,
                               Clock::time_point when) {
  if (!impl_->configured_.load()) return 0;
  uint32_t s_id = impl_->scheduler_->enqueueAt(priority, message, when);
  if (s_id > 0) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::user_request;
    impl_->reactor_queue_.tryPush(std::move(ev));
  }
  return s_id;
}

uint32_t Controller::addPollItem(uint8_t priority, ByteView message,
                                 uint32_t interval_ms) {
  uint32_t id = impl_->configured_.load() ? impl_->poll_manager_->addPollItem(
                                                priority, message, interval_ms)
                                          : 0;
  if (id != 0) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::timer_wakeup;
    impl_->reactor_queue_.tryPush(std::move(ev));
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
      OrchestrationEvent ev;
      ev.type = OrchestrationEventType::timer_wakeup;
      impl_->reactor_queue_.tryPush(std::move(ev));
      return true;
    }
  }
  return false;
}

bool Controller::scanAddresses(const std::vector<uint8_t>& addresses) {
  if (impl_->configured_.load()) {
    if (impl_->device_scanner_->scanAddresses(addresses)) {
      OrchestrationEvent ev;
      ev.type = OrchestrationEventType::timer_wakeup;
      impl_->reactor_queue_.tryPush(std::move(ev));
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

void Controller::addClient(int fd, ClientType type) {
  if (impl_->configured_.load()) impl_->client_manager_->addClient(fd, type);
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

void Controller::fetchDeviceInfo(
    std::function<void(const DeviceInfo&)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->device_manager_->fetchDeviceInfo(callback);
  }
}

void Controller::fetchMetrics(
    std::function<void(const Metrics&)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->bus_monitor_->fetchMetrics(callback);
  }
}

void Controller::fetchUtilizationHistory(
    std::function<void(float)> callback) const {
  if (impl_->configured_.load() && callback) {
    impl_->bus_monitor_->fetchUtilizationHistory(callback);
  }
}

void Controller::fetchTraceHistory(
    std::function<void(const BusEventInfo&)> callback) const {
  if (callback) {
    impl_->trace_buffer_.forEach(callback);
  }
}

void Controller::fetchErrors(
    std::function<void(const ErrorEntry&)> callback) const {
  if (callback) {
    impl_->error_buffer_.forEach(callback);
  }
}

size_t Controller::getErrorLogCapacity() const {
  return impl_->error_buffer_.capacity();
}

void Controller::fetchSystemResources(
    std::function<void(const SystemResources&)> callback) const {
  if (!callback) return;
  SystemResources res;

  res.last_update_timestamp_ms = ebus::getWallTimeMs();  // Current wall time
  res.is_configured = impl_->configured_.load();
  res.is_running = impl_->running_.load();

  auto mapThreadStatus =
      [](const detail::platform::ServiceThread::Status& s) -> ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };

  if (impl_->worker_) {
    res.threads.push_back(mapThreadStatus(impl_->worker_->status()));
  }

  if (impl_->bus_) {
    auto b_stat = impl_->bus_->getStatus();
    res.threads.push_back(b_stat.bus_thread);
    if (!b_stat.syn_thread.name.empty()) {
      res.threads.push_back(b_stat.syn_thread);
    }
  }

  res.queues.push_back({"protocol_events", impl_->protocol_events_.size(),
                        detail::ControllerLimits::event_queue_size,
                        impl_->max_protocol_events_.load()});

  res.queues.push_back({"reactor_queue", impl_->reactor_queue_.size(),
                        detail::ControllerLimits::reactor_queue_size,
                        impl_->max_reactor_queue_.load()});

  if (impl_->scheduler_) {
    res.queues.push_back(impl_->scheduler_->getStatus().queue);
  }

#if defined(ESP_PLATFORM)
  res.memory =
      MemoryStatus(heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                   esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
#endif

  callback(res);
}

void Controller::fetchServiceStatus(const JsonChunkVisitor& visitor,
                                    bool reset_histories, bool pretty) const {
  ServiceStatus snapshot;  // This is a local copy, no mutex needed for it
  {
    detail::platform::LockGuard<detail::platform::Mutex> lock(
        impl_->status_mutex_);
    snapshot = impl_->status_cache_;
  }
  serializeServiceStatus(visitor, snapshot, impl_->bus_monitor_.get(),
                         reset_histories, pretty);
}

void Controller::resetMetrics() {
  if (impl_->configured_.load()) impl_->bus_monitor_->resetMetrics();
}

void Controller::clearErrors() { impl_->error_buffer_.clear(); }

#if EBUS_SIMULATION
VirtualBus& Controller::getVirtualBus() { return *impl_->virtual_bus_; }
#endif

void Impl::getServiceStatus(ServiceStatus& status) const {
  status.last_update_timestamp_ms = ebus::getWallTimeMs();

  auto mapThreadStatus =
      [](const detail::platform::ServiceThread::Status& s) -> ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };

  // Controller's own thread status
  if (worker_) {
    status.controller.thread = mapThreadStatus(worker_->status());
  }

  if (bus_monitor_) {
    status.controller.reactor_queue_size = reactor_queue_.size();
    bus_monitor_->fetchMetrics([&](const Metrics& m) {
      status.controller.max_reactor_queue_size =
          m.controller.max_reactor_queue_size;
      status.controller.event_queue_dropped = m.controller.event_queue_dropped;
      status.controller.max_loop_cycle_us = m.controller.max_loop_cycle_us;
    });
  }

  if (configured_.load()) {
    status.bus = bus_->getStatus();
    status.bus_handler = bus_handler_->getStatus();
    status.scheduler = scheduler_->getStatus();
    status.client_manager = client_manager_->getStatus();
    status.device_manager = device_manager_->getStatus();
    status.device_scanner = device_scanner_->getStatus();
    status.poll_manager = poll_manager_->getStatus();
#if defined(ESP_PLATFORM)
    status.memory = MemoryStatus(heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                                 esp_get_free_heap_size(),
                                 esp_get_minimum_free_heap_size());
#endif
  }
}

void Impl::processPublicEvents() {
  // Capture callbacks once to avoid mutex contention inside the drainage
  // loops.
  ProtocolCallback
      user_callback;  // This is a local copy, no mutex needed for it
  {
    detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
        config_mutex_);
    user_callback = user_protocol_callback_;
  }

  ProtocolEvent ev;
#if defined(ESP_PLATFORM)
  uint8_t count = 0;
#endif

  // Process unified event queue in chronological order
  while (protocol_events_.tryPop(ev)) {
    if (user_callback) {
      ProtocolInfo info;
      info.is_error = (ev.type == ProtocolEvent::Type::error);
      info.session_id = ev.session_id;
      info.poll_id = ev.poll_id;
      info.retry_count = ev.retry_count;
      info.handler_state = ev.handler_state;
      info.request_state = ev.request_state;
      info.master_view = ev.master;
      info.slave_view = ev.slave;

      if (info.is_error) {
        info.level = ev.data.err.level;
        info.protocol_error = ev.data.err.protocol_error;
        info.result = ev.data.err.result;
        info.sequence_state = ev.data.err.sequence_state;
        // Filter by log level if applicable
        if (!detail::Logger::getInstance().isEnabled(info.level)) continue;
      } else {
        info.message_type = ev.data.tel.message_type;
        info.telegram_type = ev.data.tel.telegram_type;
      }

      user_callback(info);
    }

#if defined(ESP_PLATFORM)
    if (++count > 4) {
      count = 0;
      vTaskDelay(1);
    }
#endif
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

    // Bind the scheduler's event sink to a member function of Impl
    scheduler_->setEventSink(detail::Delegate<void(OrchestrationEvent&&)>::bind<
                             Impl, &Impl::onSchedulerEvent>(this));
    scheduler_->attachHandlerCallbacks();

    // Setup internal event dispatchers
    scheduler_->setProtocolCallback([this, owner](const ProtocolInfo& info) {
      // 1. Pack event into the decoupling queue
      ProtocolEvent ev;
      ev.type = info.is_error ? ProtocolEvent::Type::error
                              : ProtocolEvent::Type::telegram;
      ev.session_id = info.session_id;
      ev.poll_id = info.poll_id;
      ev.retry_count = info.retry_count;
      ev.handler_state = info.handler_state;
      ev.request_state = info.request_state;
      ev.master.assign(info.master_view.data(), info.master_view.size());
      ev.slave.assign(info.slave_view.data(), info.slave_view.size());
      if (info.is_error) {
        ev.data.err.level = info.level;
        ev.data.err.protocol_error = info.protocol_error;
        ev.data.err.result = info.result;
        ev.data.err.sequence_state = info.sequence_state;
      } else {
        ev.data.tel.message_type = info.message_type;
        ev.data.tel.telegram_type = info.telegram_type;
      }

      if (!protocol_events_.tryPush(ev)) {
        ProtocolEvent dummy;
        if (protocol_events_.tryPop(dummy)) {
          protocol_events_.tryPush(std::move(ev));
        }
        bus_monitor_->updateController(
            [](auto& m) { m.event_queue_dropped++; });
      } else {
        updateMaxAtomic(max_protocol_events_, protocol_events_.size());

        // Signal the reactor loop that a callback is ready for dispatch
        OrchestrationEvent callback_ev;
        callback_ev.type = OrchestrationEventType::callback_ready;

        reactor_queue_.tryPush(std::move(callback_ev));
      }

      // 2. Internal Side Effects
      if (!info.is_error) {
        if (device_manager_)
          device_manager_->update(info.master_view, info.slave_view);

        bool response_enabled = false;
        uint8_t own_address = 0xff;
        {
          detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
              config_mutex_);
          response_enabled = owner->config_.runtime.system_response;
          own_address = owner->config_.runtime.address;
        }

        if (response_enabled) {
          // Inquiry of Existence (Service 07h FEh): PB=07, SB=FE, NN=00
          if (ebus::matches(info.master_view,
                            ebus::Sequence::inquiryOfExistence(), 1)) {
            if (info.master_view[0] != own_address) {
              owner->enqueue(detail::DeviceLimits::scan_priority,
                             ebus::Sequence::signOfLife());
            }
          }
        }
      } else {
        // Diagnostic Logging
        ErrorEntry entry;
        entry.session_id = info.session_id;
        entry.poll_id = info.poll_id;
        entry.level = info.level;
        entry.protocol_error = info.protocol_error;
        entry.result = info.result;
        entry.sequence_state = info.sequence_state;
        entry.handler_state = info.handler_state;
        entry.request_state = info.request_state;
        entry.setMaster(info.master_view.data(), info.master_view.size());
        entry.setSlave(info.slave_view.data(), info.slave_view.size());
        entry.timestamp = ebus::getWallTimeMs();
        error_buffer_.push_back(std::move(entry));
      }
    });
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

    // Bridge Physical Bus Events -> Unified Reactor Queue
    bus_->addBusEventListener(
        detail::Delegate<void(
            const detail::BusEvent&)>::bind<Impl, &Impl::onBusEvent>(this));
    bus_handler_->addByteListener(
        detail::Delegate<void(
            const BusEventInfo&)>::bind<Impl, &Impl::onBusEventInfo>(this));
  }

  if (!client_manager_) {
    client_manager_ = std::make_unique<detail::ClientManager>(
        bus_.get(), bus_handler_.get(), request_.get(), bus_monitor_.get());
    client_manager_->setBusyPredicate(
        detail::Delegate<bool()>::bind<Impl, &Impl::isHandlerBusy>(this));
  }

  // Centralized synchronization using setters. Recursive mutex allows this
  // safely.
  owner->setLogLevel(owner->config_.runtime.diagnostics.level);
  owner->setAddress(owner->config_.runtime.address);
  owner->setLockCounter(owner->config_.runtime.lock_counter);
  owner->setWindow(owner->config_.runtime.bus.window_us);
  owner->setOffset(owner->config_.runtime.bus.offset_us);
  owner->setWatchdogTimeout(owner->config_.runtime.bus.watchdog_timeout_ms);
  owner->setSessionTimeout(owner->config_.runtime.network.session_timeout_ms);
  owner->setTransmitTimeout(owner->config_.runtime.network.transmit_timeout_ms);
  owner->setOutboundBufferSize(
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

void Impl::run(Controller* owner) {
  OrchestrationEvent ev;

  auto processEvent = [&](const OrchestrationEvent& event) -> bool {
    switch (event.type) {
      case OrchestrationEventType::shutdown:
        logDebug("Shutdown signal received");
        running_.store(false, std::memory_order_release);
        return true;

      case OrchestrationEventType::bus_byte: {
        detail::BusEvent bus_ev{
            event.data.byte_data.val, event.data.byte_data.bus_request,
            event.data.byte_data.start_bit, event.data.byte_data.timestamp};
        bus_handler_->processEvent(bus_ev);
        return true;
      }

      case OrchestrationEventType::protocol_result: {
        return scheduler_->injectProtocolEvent(event.data.protocol_data);
      }
      case OrchestrationEventType::network_byte: {
        // Handle external bridge data directly via ClientManager
        client_manager_->handleBusEvent(BusEventInfo{
            event.data.byte_data.val, HandlerState::passive_receive_master,
            RequestState::observe, RequestResult::observe_data, 0,
            event.data.byte_data.timestamp});
        return true;
      }
      case OrchestrationEventType::user_request:  // Fallthrough
      case OrchestrationEventType::timer_wakeup:  // Fallthrough
      case OrchestrationEventType::callback_ready:
        return true;  // These events are primarily used to wake the pop()
                      // block
      default:
        logError("Unknown orchestration event type received in Reactor Loop: " +
                 std::to_string(static_cast<uint8_t>(event.type)));
        return false;
    }
  };

  auto last_status_update = Clock::now();
  uint32_t burst_count = 0;  // Counter for burst events

  while (running_.load()) {
    auto loop_start = Clock::now();
    bool activity = false;

    if (scheduler_->tick()) activity = true;
    if (client_manager_->tick()) activity = true;

    processPublicEvents();

    poll_manager_->processDueItems(
        [this, owner,
         activity_ptr = &activity](const detail::PollManager::Item& item) {
          if (scheduler_->enqueue(item.priority, item.message, item.poll_id))
            *activity_ptr = true;
        },
        &activity);

    if (scheduler_->queueSize() < detail::SchedulerLimits::scan_threshold) {
      auto scan_cmd = device_scanner_->nextCommand();
      if (!scan_cmd.empty() &&
          scheduler_->enqueue(detail::DeviceLimits::scan_priority, scan_cmd)) {
        activity = true;
      }
    }

    const auto next_sched = scheduler_->nextDueTime();
    const auto next_client = client_manager_->nextDueTime();
    const auto next_poll = poll_manager_->nextDueTime();
    const auto tick_limit =
        Clock::now() +
        std::chrono::milliseconds(detail::SchedulerLimits::controller_tick_ms);
    const auto next_wakeup =
        std::min({next_sched, next_client, next_poll, tick_limit});

    const auto now = Clock::now();
    uint32_t timeout_ms = 0;

    // If activity happened, don't wait at all (poll the queue)
    if (!activity && next_wakeup > now) {
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          next_wakeup - now);
      timeout_ms = static_cast<uint32_t>(duration.count());
    }

    // Block on the Unified Event Queue
    if (reactor_queue_.pop(ev, timeout_ms)) {
      if (processEvent(ev)) activity = true;

      // DRAIN loop: process all pending events before doing housekeeping
      // again. This ensures that even if a burst of bytes arrives, we catch
      // up with the protocol state immediately, prioritizing the "Hot Path"
      // over background tasks.
      while (reactor_queue_.tryPop(ev)) {
        if (processEvent(ev)) activity = true;
        if (!running_.load()) return;

        // CPU Starvation Fix: If processing a large burst, force a yield to
        // allow the IDLE task, watchdog, and lower priority threads (SYN gen)
        // to breathe.
        if (++burst_count >
            detail::ControllerLimits::reactor_yield_burst_limit) {
          burst_count = 0;
#if defined(ESP_PLATFORM)
          vTaskDelay(1);
          // Mark activity as handled so we don't double-yield at the loop end
          activity = false;
#endif
          break;
        }
      }
    } else {
      burst_count = 0;
    }

    // Ensure public events are processed even if the queue pop didn't happen
    // or if tick() queued new items. This prevents starvation of user
    // callbacks during high bus activity.
    if (activity || timeout_ms == 0) {
      processPublicEvents();
#if defined(ESP_PLATFORM)
      // Mandatory yield if we didn't block in pop() (timeout_ms == 0) or if
      // activity happened, ensuring the IDLE task and watchdog can run.
      vTaskDelay(1);
#endif
    }

    // Update loop performance metrics
    auto loop_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                             Clock::now() - loop_start)
                             .count();
    if (loop_duration >
        detail::ControllerLimits::latency_warning_threshold_us) {
      logInfo("Loop iteration latency warning: " +
              std::to_string(loop_duration) + " us. Possible starvation?");
    }

    bus_monitor_->updateController([loop_duration](auto& m) {
      if (loop_duration > m.max_loop_cycle_us)
        m.max_loop_cycle_us = static_cast<uint32_t>(loop_duration);
    });

    // Throttle status updates (e.g., max once per 100ms) to save CPU/Stack
    // Fixed: Update status even during activity if too much time has passed
    auto time_since_update = Clock::now() - last_status_update;
    if ((!activity &&
         time_since_update >
             std::chrono::milliseconds(
                 detail::ControllerLimits::status_update_interval_ms_fast)) ||
        (time_since_update >
         std::chrono::milliseconds(
             detail::ControllerLimits::status_update_interval_ms_slow))) {
      // Populate the utilization history time-series before taking the
      // snapshot
      bus_monitor_->updateUtilizationHistory();

      detail::platform::LockGuard<detail::platform::Mutex> lock(status_mutex_);
      getServiceStatus(status_cache_);
      last_status_update = Clock::now();

      // Reset the windowed loop peak metrics so the next snapshot shows the
      // peak for the upcoming interval.
      bus_monitor_->resetLoopCycle();
      bus_monitor_->resetMaxReactorQueueSize(reactor_queue_.size());
      scheduler_->resetPeakMetrics();
      device_scanner_->resetPeakMetrics();
      poll_manager_->resetPeakMetrics();
    }
  }
}

void Impl::onSchedulerEvent(OrchestrationEvent&& oev) {
  if (!reactor_queue_.tryPush(oev)) {
    // DRAIN: Make room for terminal protocol results
    OrchestrationEvent dummy;
    if (reactor_queue_.tryPop(dummy)) {
      reactor_queue_.tryPush(std::move(oev));
      bus_monitor_->updateController([](auto& m) { m.event_queue_dropped++; });
    }
  }
  size_t current_size = reactor_queue_.size();
  updateMaxAtomic(max_reactor_queue_, current_size);
  bus_monitor_->updateController([current_size](auto& m) {
    if (current_size > m.max_reactor_queue_size)
      m.max_reactor_queue_size = static_cast<uint32_t>(current_size);
  });
}

void Impl::onBusEventInfo(const BusEventInfo& info) {
  // Store in internal history
  trace_buffer_.push_back(BusEventInfo(info));

  // Invoke user callback if registered
  TraceCallback user_callback;
  {
    detail::platform::LockGuard<detail::platform::RecursiveMutex> lock(
        config_mutex_);
    user_callback = user_trace_callback_;
  }
  if (user_callback) user_callback(info);
}

void Impl::onBusEvent(const detail::BusEvent& bus_ev) {
  OrchestrationEvent ev;
  ev.type = OrchestrationEventType::bus_byte;
  ev.data.byte_data.val = bus_ev.byte;
  ev.data.byte_data.bus_request = bus_ev.bus_request;
  ev.data.byte_data.start_bit = bus_ev.start_bit;
  ev.data.byte_data.timestamp = bus_ev.timestamp;

  if (bus_ev.byte != Symbols::syn &&
      detail::Logger::getInstance().isEnabled(LogLevel::debug)) {
    char dbg_buf[48];
    std::snprintf(dbg_buf, sizeof(dbg_buf), "HAL Bridge: tryPush 0x%s",
                  ebus::toString(bus_ev.byte));
    logDebug(dbg_buf);
  }

  if (!reactor_queue_.tryPush(ev)) {
    detail::Logger::getInstance().log(
        LogLevel::error, "[Controller] Reactor queue FULL! Dropping event.");
    // DRAIN: Drop oldest byte event to ensure loop stays moving
    OrchestrationEvent dummy;
    if (reactor_queue_.tryPop(dummy)) {
      reactor_queue_.tryPush(std::move(ev));
    }
    bus_monitor_->updateController([](auto& m) { m.event_queue_dropped++; });
  }

  size_t current_size = reactor_queue_.size();
  updateMaxAtomic(max_reactor_queue_, current_size);
  bus_monitor_->updateController([current_size](auto& m) {
    if (current_size > m.max_reactor_queue_size)
      m.max_reactor_queue_size = static_cast<uint32_t>(current_size);
  });
}

bool Impl::isSchedulerFull() const {
  return scheduler_ && scheduler_->queueSize() >= scheduler_->queueCapacity();
}

bool Impl::isHandlerBusy() const {
  return handler_ && handler_->isActiveMessagePending();
}

bool Impl::isSystemBusy() const {
  return (handler_ && handler_->isActiveMessagePending()) ||
         (client_manager_ && client_manager_->isSessionActive());
}

void Impl::logError(std::string_view msg) const {
  auto& logger = detail::Logger::getInstance();
  if (!logger.isEnabled(LogLevel::error)) return;
  uint8_t addr = address_.load(std::memory_order_relaxed);
  char buf[detail::LoggerLimits::log_buffer_size];
  int n = std::snprintf(buf, sizeof(buf), "[0x%02x] %.*s", addr,
                        (int)msg.size(), msg.data());
  if (n > 0)
    logger.log(LogLevel::error,
               std::string_view(buf, std::min((size_t)n, sizeof(buf) - 1)));
}

void Impl::logInfo(std::string_view msg) const {
  auto& logger = detail::Logger::getInstance();
  if (!logger.isEnabled(LogLevel::info)) return;
  uint8_t addr = address_.load(std::memory_order_relaxed);
  char buf[256];
  int n = std::snprintf(buf, sizeof(buf), "[0x%02x] %.*s", addr,
                        (int)msg.size(), msg.data());
  if (n > 0)
    logger.log(LogLevel::info,
               std::string_view(buf, std::min((size_t)n, sizeof(buf) - 1)));
}

void Impl::logDebug(std::string_view msg) const {
  auto& logger = detail::Logger::getInstance();
  if (!logger.isEnabled(LogLevel::debug)) return;
  uint8_t addr = address_.load(std::memory_order_relaxed);
  char buf[256];
  int n = std::snprintf(buf, sizeof(buf), "[0x%02x] %.*s", addr,
                        (int)msg.size(), msg.data());
  if (n > 0)
    logger.log(LogLevel::debug,
               std::string_view(buf, std::min((size_t)n, sizeof(buf) - 1)));
}

}  // namespace ebus
