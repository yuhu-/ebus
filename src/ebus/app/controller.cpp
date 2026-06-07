/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>
#if EBUS_SIMULATION
#include <ebus/virtual_bus.hpp>
#endif
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>

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
#include "utils/circular_buffer.hpp"
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

  ebus::ReactiveMasterSlaveCallback user_reactive_callback_;
  ebus::TelegramCallback user_telegram_callback_;
  ebus::ErrorCallback user_error_callback_;
  ebus::TraceCallback user_trace_callback_;

  // Decoupled queues for user callbacks (Prioritized drainage)
  detail::platform::Queue<ebus::ProtocolEvent> event_errors_{
      detail::ControllerLimits::event_queue_size};
  detail::platform::Queue<ebus::ProtocolEvent> event_telegrams_{
      detail::ControllerLimits::event_queue_size};

  detail::platform::Queue<ebus::OrchestrationEvent> reactor_queue_{
      detail::ControllerLimits::reactor_queue_size};

  std::atomic<size_t> max_event_errors_{0};
  std::atomic<size_t> max_event_telegrams_{0};
  std::atomic<size_t> max_reactor_queue_{0};

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

  mutable std::mutex status_mutex_;  // Keep as standard mutex, no recursion
                                     // needed for status snapshots
  std::atomic<LogLevel> log_level_{LogLevel::error};
  std::atomic<uint8_t> address_{0xff};
  ServiceStatus status_cache_;
};

Controller::Controller() : impl_(new Impl()) {}

Controller::Controller(const EbusConfig& config)
    : config_(config), impl_(new Impl()) {
  constructMembers();
  configured_.store(true);
}

Controller::~Controller() { stop(); }

bool Controller::start() {
  if (!isConfigured()) return false;
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return true;

  // Reset internal queues to clear any previous shutdown state or stale events
  impl_->reactor_queue_.reset();
  impl_->event_errors_.reset();
  impl_->event_telegrams_.reset();

  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  impl_->bus_->start();
  impl_->client_manager_->start();

  // Trigger initial system discovery if enabled
  if (config_.runtime.system_inquiry) triggerInquiryOfExistence();

  impl_->worker_ = std::make_unique<detail::platform::ServiceThread>(
      "ebus_controller", [this] { run(); },
      detail::OrchestrationLimits::controller_stack_size,
      detail::OrchestrationLimits::controller_priority);
  impl_->worker_->start();

  return true;
}

void Controller::stop() {
  logInfo("Stopping controller services...");
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) return;

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
  if (running_.load()) {
    if (detail::ConfigValidator::requiresHardwareRestart(config_, config)) {
      return false;  // Cannot change physical UART while running
    }
  }

  // 3. Apply changes
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_ = config;
  constructMembers();
  configured_.store(true);
  return true;
}

EbusConfig Controller::getConfig() const {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  return config_;
}

void Controller::setAddress(const uint8_t& address) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.address = address;
  impl_->address_.store(address, std::memory_order_relaxed);
  if (isConfigured()) {
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
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.lock_counter = lock_counter;
  if (isConfigured()) {
    impl_->request_->setLockCounter(lock_counter);
  }
}

void Controller::setSystemInquiry(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.system_inquiry = enable;
}

void Controller::setSystemResponse(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.system_response = enable;
}

void Controller::setWindow(const uint16_t& window_us) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.bus.window_us = window_us;
  if (isConfigured()) impl_->bus_->setWindow(window_us);
}

void Controller::setOffset(const uint16_t& offset_us) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.bus.offset_us = offset_us;
  if (isConfigured()) impl_->bus_->setOffset(offset_us);
}

void Controller::setWatchdogTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.bus.watchdog_timeout_ms = timeout_ms;
  if (isConfigured()) {
    impl_->bus_handler_->setWatchdogTimeout(timeout_ms);
  }
}

void Controller::setLogLevel(LogLevel level) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.diagnostics.level = level;
  impl_->log_level_.store(level, std::memory_order_relaxed);
  // In multi-controller environments (sim), we keep the most verbose level
  // requested.
  auto current = detail::Logger::getInstance().getLevel();
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(current)) {
    detail::Logger::getInstance().setLevel(level);
  }
}

void Controller::setLogSink(
    std::function<void(LogLevel, std::string_view)> sink) {
  detail::Logger::getInstance().setSink(std::move(sink));
}

void Controller::setErrorLogSize(size_t size) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.diagnostics.log_size = size;
  if (isConfigured()) impl_->error_buffer_.clear();  // Reset log on size change
}

void Controller::setSessionTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.network.session_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->client_manager_->setSessionTimeout(timeout_ms);
}

void Controller::setTransmitTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.network.transmit_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->client_manager_->setTransmitTimeout(timeout_ms);
}

void Controller::setOutboundBufferSize(size_t size) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.network.outbound_buffer_size = size;
  if (isConfigured()) {
    impl_->client_manager_->setOutboundBufferSize(size);
  }
}

void Controller::setScanOnStartup(bool enable) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.device.scan_on_startup = enable;
  if (isConfigured()) impl_->device_scanner_->setScanOnStartup(enable);
}

void Controller::setMaxStartupScans(uint8_t max_scans) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.device.max_startup_scans = max_scans;
  if (isConfigured()) {
    impl_->device_scanner_->setMaxStartupScans(max_scans);
  }
}

void Controller::setInitialScanDelay(uint32_t delay_s) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.device.initial_delay_s = delay_s;
  if (isConfigured()) {
    impl_->device_scanner_->setInitialScanDelay(delay_s);
  }
}

void Controller::setStartupScanInterval(uint32_t interval_s) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.device.startup_interval_s = interval_s;
  if (isConfigured()) {
    impl_->device_scanner_->setStartupScanInterval(interval_s);
  }
}

void Controller::setMaxSendAttempts(uint8_t max_send_attempts) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.scheduler.max_send_attempts = max_send_attempts;
  if (isConfigured()) {
    impl_->scheduler_->setMaxSendAttempts(max_send_attempts);
  }
}

void Controller::setBaseBackoff(uint32_t base_backoff_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.scheduler.base_backoff_ms = base_backoff_ms;
  if (isConfigured()) impl_->scheduler_->setBaseBackoff(base_backoff_ms);
}

void Controller::setFsmTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.scheduler.fsm_timeout_ms = timeout_ms;
  if (isConfigured()) {
    impl_->scheduler_->setFsmTimeout(timeout_ms);
  }
}

void Controller::setTotalTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  config_.runtime.scheduler.total_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->scheduler_->setTotalTimeout(timeout_ms);
}

void Controller::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  impl_->user_reactive_callback_ = std::move(callback);
  if (isConfigured())
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
}

void Controller::setTelegramCallback(TelegramCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  impl_->user_telegram_callback_ = std::move(callback);
}

void Controller::setErrorCallback(ErrorCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  impl_->user_error_callback_ = std::move(callback);
}

void Controller::setTraceCallback(TraceCallback callback) {
  std::lock_guard<std::recursive_mutex> lock(config_mutex_);
  impl_->user_trace_callback_ = std::move(callback);
}

bool Controller::enqueue(uint8_t priority, ByteView message,
                         ResultCallback callback) {
  if (!isConfigured()) return false;
  bool ok = impl_->scheduler_->enqueue(priority, message, std::move(callback));
  if (ok) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::user_request;
    impl_->reactor_queue_.tryPush(std::move(ev));
  }
  return ok;
}

bool Controller::enqueueAt(uint8_t priority, ByteView message,
                           Clock::time_point when, ResultCallback callback) {
  if (!isConfigured()) return false;
  bool ok = impl_->scheduler_->enqueueAt(priority, message, when,
                                         std::move(callback));
  if (ok) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::user_request;
    impl_->reactor_queue_.tryPush(std::move(ev));
  }
  return ok;
}

uint32_t Controller::addPollItem(uint8_t priority, ByteView message,
                                 uint32_t interval_ms,
                                 ResultCallback callback) {
  uint32_t id = isConfigured()
                    ? impl_->poll_manager_->addPollItem(
                          priority, message, interval_ms, std::move(callback))
                    : 0;
  if (id != 0) {
    OrchestrationEvent ev;
    ev.type = OrchestrationEventType::timer_wakeup;
    impl_->reactor_queue_.tryPush(std::move(ev));
  }
  return id;
}

void Controller::removePollItem(uint32_t id) {
  if (isConfigured()) impl_->poll_manager_->removePollItem(id);
}

void Controller::clearPollItems() {
  if (isConfigured()) impl_->poll_manager_->clear();
}

void Controller::triggerInquiryOfExistence() {
  enqueue(detail::DeviceLimits::scan_priority,
          ebus::Sequence::InquiryOfExistence());
}

void Controller::initFullScan(bool enable) {
  if (isConfigured()) impl_->device_scanner_->initFullScan(enable);
}

bool Controller::scanAddress(uint8_t address) {
  if (isConfigured()) {
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
  if (isConfigured()) {
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
  return isConfigured() ? impl_->device_scanner_->scanObservedDevices() : false;
}

void Controller::addClient(int fd, ClientType type) {
  if (isConfigured()) impl_->client_manager_->addClient(fd, type);
}

void Controller::removeClient(int fd) {
  if (isConfigured()) impl_->client_manager_->removeClient(fd);
}

bool Controller::isRunning() const noexcept { return running_.load(); }

bool Controller::isConfigured() const noexcept { return configured_.load(); }

bool Controller::isScanning() const {
  return isConfigured() ? impl_->device_scanner_->isScanning() : false;
}

void Controller::fetchDeviceInfo(
    std::function<void(const DeviceInfo&)> callback) const {
  if (isConfigured() && callback) {
    impl_->device_manager_->fetchDeviceInfo(callback);
  }
}

void Controller::fetchMetrics(
    std::function<void(const Metrics&)> callback) const {
  if (isConfigured() && callback) {
    impl_->bus_monitor_->fetchMetrics(callback);
  }
}

void Controller::fetchUtilizationHistory(
    std::function<void(float)> callback) const {
  if (isConfigured() && callback) {
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

  res.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  res.is_configured = isConfigured();
  res.is_running = isRunning();

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

  res.queues.push_back({"event_errors", impl_->event_errors_.size(),
                        detail::ControllerLimits::event_queue_size,
                        impl_->max_event_errors_.load()});
  res.queues.push_back({"event_telegrams", impl_->event_telegrams_.size(),
                        detail::ControllerLimits::event_queue_size,
                        impl_->max_event_telegrams_.load()});

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
  ServiceStatus snapshot;
  {
    std::lock_guard<std::mutex> lock(impl_->status_mutex_);
    snapshot = impl_->status_cache_;
  }
  serializeServiceStatus(visitor, snapshot, impl_->bus_monitor_.get(),
                         reset_histories, pretty);
}

void Controller::resetMetrics() {
  if (isConfigured()) impl_->bus_monitor_->resetMetrics();
}

void Controller::clearErrors() { impl_->error_buffer_.clear(); }

#if EBUS_SIMULATION
VirtualBus& Controller::getVirtualBus() { return *impl_->virtual_bus_; }
#endif

void Controller::getServiceStatus(ServiceStatus& status) const {
  status.last_update_timestamp_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  auto mapThreadStatus =
      [](const detail::platform::ServiceThread::Status& s) -> ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };

  // Controller's own thread status
  if (impl_->worker_) {
    status.controller.thread = mapThreadStatus(impl_->worker_->status());
  }

  if (impl_->bus_monitor_) {
    status.controller.reactor_queue_size = impl_->reactor_queue_.size();
    impl_->bus_monitor_->fetchMetrics([&](const Metrics& m) {
      status.controller.max_reactor_queue_size =
          m.controller.max_reactor_queue_size;
      status.controller.event_queue_dropped = m.controller.event_queue_dropped;
      status.controller.max_loop_cycle_us = m.controller.max_loop_cycle_us;
    });
  }

  if (isConfigured()) {
    status.bus = impl_->bus_->getStatus();
    status.bus_handler = impl_->bus_handler_->getStatus();
    status.scheduler = impl_->scheduler_->getStatus();
    status.client_manager = impl_->client_manager_->getStatus();
    status.device_manager = impl_->device_manager_->getStatus();
    status.device_scanner = impl_->device_scanner_->getStatus();
    status.poll_manager = impl_->poll_manager_->getStatus();
#if defined(ESP_PLATFORM)
    status.memory = MemoryStatus(heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                                 esp_get_free_heap_size(),
                                 esp_get_minimum_free_heap_size());
#endif
  }
}

void Controller::processPublicEvents() {
  // Capture callbacks once to avoid mutex contention inside the drainage loops.
  // std::function is safe to copy and call outside the lock.
  ErrorCallback err_callback;
  TelegramCallback tel_callback;
  {
    std::lock_guard<std::recursive_mutex> lock(config_mutex_);
    err_callback = impl_->user_error_callback_;
    tel_callback = impl_->user_telegram_callback_;
  }

  ProtocolEvent ev;
#if defined(ESP_PLATFORM)
  uint8_t count = 0;
#endif
  // 1. Prioritize Errors: Drain all pending error events first
  while (impl_->event_errors_.tryPop(ev)) {
    const auto& callback = err_callback;
    if (callback &&
        detail::Logger::getInstance().isEnabled(ev.data.err.level)) {
      ErrorInfo info{ev.session_id,      ev.poll_id,
                     ev.data.err.level,  ev.data.err.protocol_error,
                     ev.data.err.result, ev.data.err.sequence_state,
                     ev.handler_state,   ev.request_state,
                     ev.master,          ev.slave};
      callback(info);
    }

#if defined(ESP_PLATFORM)
    // Cooperative yielding during burst of callbacks to allow lower priority
    // threads (like SYN generator) to run.
    if (++count > 4) {
      count = 0;
      vTaskDelay(1);
    }
#endif
  }

  // 2. Process Telegrams
  while (impl_->event_telegrams_.tryPop(ev)) {
    const auto& callback = tel_callback;
    if (callback) {
      TelegramInfo info{ev.session_id,
                        ev.poll_id,
                        ev.data.tel.retry_count,
                        ev.data.tel.message_type,
                        ev.data.tel.telegram_type,
                        ev.handler_state,
                        ev.request_state,
                        ev.master,
                        ev.slave};
      callback(info);
    }

#if defined(ESP_PLATFORM)
    // Cooperative yielding during burst of callbacks
    if (++count > 4) {
      count = 0;
      vTaskDelay(1);
    }
#endif
  }
}

void Controller::constructMembers() {
  // -- 1. Telemetry & Core Arbitration --
  if (!impl_->bus_monitor_) {
    impl_->bus_monitor_ = std::make_unique<detail::BusMonitor>();
  }

  if (!impl_->request_) {
    impl_->request_ =
        std::make_unique<detail::Request>(impl_->bus_monitor_.get());
  }

  // -- 2. Physical Layer --
  // Note: configure() ensures we don't change hardware params while running
  if (!impl_->bus_) {
    impl_->bus_ = std::make_unique<detail::platform::Bus>(
        config_.bus, config_.runtime, impl_->request_.get(),
        impl_->bus_monitor_.get());
  }

#if EBUS_SIMULATION
  // Initialize VirtualBus build flag is set
  if (!impl_->virtual_bus_) {
    impl_->virtual_bus_ =
        std::unique_ptr<ebus::VirtualBus>(new ebus::VirtualBus(*impl_->bus_));
  }
#endif

  // -- 3. Protocol Handler --
  if (!impl_->handler_) {
    impl_->handler_ = std::make_unique<detail::Handler>(
        config_.runtime.address, impl_->bus_.get(), impl_->request_.get(),
        impl_->bus_monitor_.get());
  }

  // -- 4. Scheduler --
  if (!impl_->scheduler_) {
    impl_->scheduler_ =
        std::make_unique<detail::Scheduler>(impl_->handler_.get());

    impl_->scheduler_->setEventSink([this](OrchestrationEvent&& oev) {
      if (!impl_->reactor_queue_.tryPush(oev)) {
        // DRAIN: Make room for terminal protocol results
        OrchestrationEvent dummy;
        if (impl_->reactor_queue_.tryPop(dummy)) {
          impl_->reactor_queue_.tryPush(std::move(oev));
          impl_->bus_monitor_->updateController(
              [](auto& m) { m.event_queue_dropped++; });
        }
      }
      size_t current_size = impl_->reactor_queue_.size();
      updateMaxAtomic(impl_->max_reactor_queue_, current_size);
      impl_->bus_monitor_->updateController([current_size](auto& m) {
        if (current_size > m.max_reactor_queue_size)
          m.max_reactor_queue_size = static_cast<uint32_t>(current_size);
      });
    });
    impl_->scheduler_->attachHandlerCallbacks();

    // Setup internal event dispatchers
    impl_->scheduler_->setTelegramCallback([this](const TelegramInfo& info) {
      // 1. Pack event into the decoupling queue
      ProtocolEvent ev;
      ev.type = ProtocolEvent::Type::telegram;
      ev.session_id = info.session_id;
      ev.poll_id = info.poll_id;
      ev.data.tel.retry_count = info.retry_count;
      ev.data.tel.message_type = info.message_type;
      ev.data.tel.telegram_type = info.telegram_type;
      ev.handler_state = info.handler_state;
      ev.request_state = info.request_state;
      ev.master.assign(info.master_view.data(), info.master_view.size());
      ev.slave.assign(info.slave_view.data(), info.slave_view.size());
      if (!impl_->event_telegrams_.tryPush(ev)) {
        // DRAIN: Drop oldest telegram if public queue is full
        ProtocolEvent dummy;
        if (impl_->event_telegrams_.tryPop(dummy)) {
          impl_->event_telegrams_.tryPush(std::move(ev));
        }
        impl_->bus_monitor_->updateController(
            [](auto& m) { m.event_queue_dropped++; });
      } else {
        // Update high watermark
        updateMaxAtomic(impl_->max_event_telegrams_,
                        impl_->event_telegrams_.size());

        // Signal the reactor loop that a callback is ready for dispatch
        OrchestrationEvent callback_ev;
        callback_ev.type = OrchestrationEventType::callback_ready;
        if (!impl_->reactor_queue_.tryPush(std::move(callback_ev))) {
          impl_->bus_monitor_->updateController(
              [](auto& m) { m.event_queue_dropped++; });
        }
      }

      // 2. Handle internal discovery logic immediately
      bool response_enabled = false;
      uint8_t own_address = 0xff;
      {
        std::lock_guard<std::recursive_mutex> lock(config_mutex_);
        response_enabled = config_.runtime.system_response;
        own_address = config_.runtime.address;
      }

      if (impl_->device_manager_) {
        impl_->device_manager_->update(info.master_view, info.slave_view);
      }

      // Standard eBUS System Discovery reaction
      if (response_enabled) {
        // Inquiry of Existence (Service 07h FEh): PB=07, SB=FE, NN=00
        // We match starting at index 1 to verify ZZ, PB, SB, and NN.
        if (ebus::matches(info.master_view,
                          ebus::Sequence::InquiryOfExistence(), 1)) {
          const uint8_t source = info.master_view[0];

          // Don't respond to our own inquiries.
          if (source != own_address) {
            impl_->scheduler_->enqueue(detail::DeviceLimits::scan_priority,
                                       ebus::Sequence::SignOfLife());
          }
        }
      }
    });

    impl_->scheduler_->setErrorCallback([this](const ErrorInfo& info) {
      // 1. Pack event into the decoupling queue
      ProtocolEvent ev;
      ev.type = ProtocolEvent::Type::error;
      ev.session_id = info.session_id;
      ev.poll_id = info.poll_id;
      ev.data.err.level = info.level;
      ev.data.err.protocol_error = info.protocol_error;
      ev.data.err.result = info.result;
      ev.data.err.sequence_state = info.sequence_state;
      ev.handler_state = info.handler_state;
      ev.request_state = info.request_state;
      ev.master.assign(info.master_view.data(), info.master_view.size());
      ev.slave.assign(info.slave_view.data(), info.slave_view.size());
      if (!impl_->event_errors_.tryPush(ev)) {
        // DRAIN: Drop oldest error if public queue is full
        ProtocolEvent dummy;
        if (impl_->event_errors_.tryPop(dummy)) {
          impl_->event_errors_.tryPush(std::move(ev));
        }
        impl_->bus_monitor_->updateController(
            [](auto& m) { m.event_queue_dropped++; });
      } else {
        // Update high watermark
        updateMaxAtomic(impl_->max_event_errors_, impl_->event_errors_.size());
      }

      // Signal the reactor loop that a callback is ready for dispatch
      OrchestrationEvent callback_ev;
      callback_ev.type = OrchestrationEventType::callback_ready;
      if (!impl_->reactor_queue_.tryPush(std::move(callback_ev))) {
        impl_->bus_monitor_->updateController(
            [](auto& m) { m.event_queue_dropped++; });
      }

      // 2. Handle internal diagnostic logging. The ConfigValidator ensures
      // log_size > 0.
      {
        ErrorEntry entry;
        entry.session_id = info.session_id;
        entry.poll_id = info.poll_id;
        entry.level = info.level;  // LogLevel is still used for filtering
        entry.protocol_error = info.protocol_error;
        entry.result = info.result;
        entry.sequence_state = info.sequence_state;
        entry.handler_state = info.handler_state;
        entry.request_state = info.request_state;
        entry.setMaster(info.master_view.data(), info.master_view.size());
        entry.setSlave(info.slave_view.data(), info.slave_view.size());
        entry.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        impl_->error_buffer_.push_back(std::move(entry));
      }
    });
  }

  // -- 5. Application Logic --
  if (!impl_->device_manager_) {
    impl_->device_manager_ =
        std::make_unique<detail::DeviceManager>(impl_->bus_monitor_.get());
  }

  if (!impl_->device_scanner_) {
    impl_->device_scanner_ = std::make_unique<detail::DeviceScanner>(
        config_.runtime.address, impl_->device_manager_.get());
  }

  if (!impl_->poll_manager_) {
    impl_->poll_manager_ = std::make_unique<detail::PollManager>();
  }

  // -- 6. Plumbing --
  if (!impl_->bus_handler_) {
    impl_->bus_handler_ = std::make_unique<detail::BusHandler>(
        impl_->request_.get(), impl_->handler_.get());

    // Bridge Physical Bus Events -> Unified Reactor Queue
    impl_->bus_->addBusEventListener([this](const detail::BusEvent& bus_ev) {
      OrchestrationEvent ev;
      ev.type = OrchestrationEventType::bus_byte;
      ev.data.byte_data.val = bus_ev.byte;
      ev.data.byte_data.bus_request = bus_ev.bus_request;
      ev.data.byte_data.start_bit = bus_ev.start_bit;
      ev.data.byte_data.timestamp = bus_ev.timestamp;
      if (bus_ev.byte != Symbols::syn) {
        logDebug("HAL Bridge: tryPush 0x" + ebus::toString(bus_ev.byte));
      }
      if (!impl_->reactor_queue_.tryPush(ev)) {
        logError("[Controller] Reactor queue FULL! Dropping event.");
        // DRAIN: Drop oldest byte event to ensure loop stays moving
        OrchestrationEvent dummy;
        if (impl_->reactor_queue_.tryPop(dummy)) {
          impl_->reactor_queue_.tryPush(std::move(ev));
        }
        impl_->bus_monitor_->updateController(
            [](auto& m) { m.event_queue_dropped++; });
      }

      size_t current_size = impl_->reactor_queue_.size();
      updateMaxAtomic(impl_->max_reactor_queue_, current_size);
      impl_->bus_monitor_->updateController([current_size](auto& m) {
        if (current_size > m.max_reactor_queue_size)
          m.max_reactor_queue_size = static_cast<uint32_t>(current_size);
      });
    });

    // Add the permanent tracing listener
    impl_->bus_handler_->addByteListener([this](const BusEventInfo& info) {
      // Store in internal history
      impl_->trace_buffer_.push_back(BusEventInfo(info));

      // Invoke user callback if registered
      TraceCallback user_callback;
      {
        std::lock_guard<std::recursive_mutex> lock(config_mutex_);
        user_callback = impl_->user_trace_callback_;
      }
      if (user_callback) user_callback(info);
    });
  }

  if (!impl_->client_manager_) {
    impl_->client_manager_ = std::make_unique<detail::ClientManager>(
        impl_->bus_.get(), impl_->bus_handler_.get(), impl_->request_.get(),
        impl_->bus_monitor_.get());
  }

  // Centralized synchronization using setters. Recursive mutex allows this
  // safely.
  setLogLevel(config_.runtime.diagnostics.level);
  setAddress(config_.runtime.address);
  setLockCounter(config_.runtime.lock_counter);
  setWindow(config_.runtime.bus.window_us);
  setOffset(config_.runtime.bus.offset_us);
  setWatchdogTimeout(config_.runtime.bus.watchdog_timeout_ms);
  setSessionTimeout(config_.runtime.network.session_timeout_ms);
  setTransmitTimeout(config_.runtime.network.transmit_timeout_ms);
  setOutboundBufferSize(config_.runtime.network.outbound_buffer_size);
  setScanOnStartup(config_.runtime.device.scan_on_startup);
  setMaxStartupScans(config_.runtime.device.max_startup_scans);
  setInitialScanDelay(config_.runtime.device.initial_delay_s);
  setStartupScanInterval(config_.runtime.device.startup_interval_s);
  setMaxSendAttempts(config_.runtime.scheduler.max_send_attempts);
  setBaseBackoff(config_.runtime.scheduler.base_backoff_ms);
  setFsmTimeout(config_.runtime.scheduler.fsm_timeout_ms);
  setTotalTimeout(config_.runtime.scheduler.total_timeout_ms);
  if (impl_->user_reactive_callback_) {
    setReactiveMasterSlaveCallback(impl_->user_reactive_callback_);
  }
}

void Controller::run() {
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
        impl_->bus_handler_->processEvent(bus_ev);
        return true;
      }

      case OrchestrationEventType::protocol_result: {
        return impl_->scheduler_->injectProtocolEvent(event.data.protocol_data);
      }
      case OrchestrationEventType::network_byte: {
        // Handle external bridge data directly via ClientManager
        impl_->client_manager_->handleBusEvent(BusEventInfo{
            event.data.byte_data.val, HandlerState::passive_receive_master,
            RequestState::observe, RequestResult::observe_data, 0,
            event.data.byte_data.timestamp});
        return true;
      }
      case OrchestrationEventType::user_request:  // Fallthrough
      case OrchestrationEventType::timer_wakeup:  // Fallthrough
      case OrchestrationEventType::callback_ready:
        return true;  // These events are primarily used to wake the pop() block
      default:
        logError("Unknown orchestration event type received in Reactor Loop: " +
                 std::to_string(static_cast<uint8_t>(event.type)));
        return false;
    }
  };

  auto last_status_update = Clock::now();
  uint32_t burst_count = 0;

  while (running_.load()) {
    auto loop_start = Clock::now();
    bool activity = false;

    if (impl_->scheduler_->tick()) activity = true;
    if (impl_->client_manager_->tick()) activity = true;

    processPublicEvents();

    impl_->poll_manager_->processDueItems(
        [this, &activity](const detail::PollManager::Item& item) {
          if (impl_->scheduler_->enqueue(item.priority, item.message,
                                         item.callback, item.poll_id))
            activity = true;
        },
        &activity);

    if (impl_->scheduler_->queueSize() <
        detail::SchedulerLimits::scan_threshold) {
      auto scan_cmd = impl_->device_scanner_->nextCommand();
      if (!scan_cmd.empty()) {
        if (impl_->scheduler_->enqueue(
                detail::DeviceLimits::scan_priority, scan_cmd,
                [this](const ebus::ResultInfo& info) {
                  if (!info.success &&
                      (info.result == RequestResult::first_lost ||
                       info.result == RequestResult::second_lost)) {
                    // Re-enqueue address for scan if we lost arbitration.
                    // Note: We whiltelist arbitration loss to avoid re-probing
                    // on structural protocol errors (SequenceState) which would
                    // be futile. Noise (first_error) is handled by the
                    // Scheduler's own retry logic.
                    if (info.master_view.size() > 1) {
                      impl_->device_scanner_->scanAddress(info.master_view[1]);
                    }
                  }
                })) {
          activity = true;
        }
      }
    }

    const auto next_sched = impl_->scheduler_->nextDueTime();
    const auto next_client = impl_->client_manager_->nextDueTime();
    const auto next_poll = impl_->poll_manager_->nextDueTime();
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
    if (impl_->reactor_queue_.pop(ev, timeout_ms)) {
      if (processEvent(ev)) activity = true;

      // DRAIN loop: process all pending events before doing housekeeping again.
      // This ensures that even if a burst of bytes arrives, we catch up with
      // the protocol state immediately, prioritizing the "Hot Path" over
      // background tasks.
      while (impl_->reactor_queue_.tryPop(ev)) {
        if (processEvent(ev)) activity = true;
        if (!running_.load()) return;

        // CPU Starvation Fix: If processing a large burst, force a yield to
        // allow the IDLE task, watchdog, and lower priority threads (SYN gen)
        // to breathe.
        if (++burst_count > 10) {
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
    if (activity) {
      processPublicEvents();
    }

#if defined(ESP_PLATFORM)
    // Mandatory yield on single-core if activity happened to ensure fairness.
    // If pop() blocked above, the OS already handled yielding.
    if (activity) {
      vTaskDelay(1);
    }
#endif

    // Update loop performance metrics
    auto loop_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                             Clock::now() - loop_start)
                             .count();
    if (loop_duration > 100000) {  // 100ms warning
      logInfo("Loop iteration latency warning: " +
              std::to_string(loop_duration) + " us. Possible starvation?");
    }

    impl_->bus_monitor_->updateController([loop_duration](auto& m) {
      if (loop_duration > m.max_loop_cycle_us)
        m.max_loop_cycle_us = static_cast<uint32_t>(loop_duration);
    });

    // Throttle status updates (e.g., max once per 100ms) to save CPU/Stack
    // Fixed: Update status even during activity if too much time has passed
    auto time_since_update = Clock::now() - last_status_update;
    if ((!activity && time_since_update > std::chrono::milliseconds(100)) ||
        (time_since_update > std::chrono::milliseconds(500))) {
      std::lock_guard<std::mutex> lock(impl_->status_mutex_);
      getServiceStatus(impl_->status_cache_);
      last_status_update = Clock::now();

      // Reset the windowed loop peak metrics so the next snapshot shows the
      // peak for the upcoming interval.
      impl_->bus_monitor_->resetLoopCycle();
      impl_->bus_monitor_->resetMaxReactorQueueSize(
          impl_->reactor_queue_.size());
      impl_->scheduler_->resetPeakMetrics();
      impl_->device_scanner_->resetPeakMetrics();
      impl_->poll_manager_->resetPeakMetrics();
    }
  }
}

void Controller::logError(const std::string& msg) const {
  uint8_t addr = impl_->address_.load(std::memory_order_relaxed);
  detail::Logger::getInstance().log(LogLevel::error,
                                    "[0x" + ebus::toString(addr) + "] " + msg);
}

void Controller::logInfo(const std::string& msg) const {
  uint8_t addr = impl_->address_.load(std::memory_order_relaxed);
  detail::Logger::getInstance().log(LogLevel::info,
                                    "[0x" + ebus::toString(addr) + "] " + msg);
}

void Controller::logDebug(const std::string& msg) const {
  if (impl_->log_level_.load(std::memory_order_relaxed) < LogLevel::debug)
    return;
  uint8_t addr = impl_->address_.load(std::memory_order_relaxed);
  detail::Logger::getInstance().log(LogLevel::debug,
                                    "[0x" + ebus::toString(addr) + "] " + msg);
}

}  // namespace ebus
