/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/controller.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>
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

namespace ebus {

struct Impl {
  detail::CircularBuffer<ebus::ErrorEntry> error_buffer_{
      ebus::RuntimeConfig{}.logging.log_size};
  detail::CircularBuffer<ebus::BusEventContext> trace_buffer_{100};

  ebus::ReactiveMasterSlaveCallback user_reactive_callback_;
  ebus::TelegramCallback user_telegram_callback_;
  ebus::ErrorCallback user_error_callback_;
  ebus::TraceCallback user_trace_callback_;

  std::unique_ptr<detail::Request> request_;
  std::unique_ptr<detail::BusMonitor> bus_monitor_;
  std::unique_ptr<detail::platform::Bus> bus_;
  std::unique_ptr<detail::BusHandler> bus_handler_;
  std::unique_ptr<detail::Handler> handler_;
  std::unique_ptr<detail::DeviceManager> device_manager_;
  std::unique_ptr<detail::DeviceScanner> device_scanner_;
  std::unique_ptr<detail::PollManager> poll_manager_;
  std::unique_ptr<detail::Scheduler> scheduler_;
  std::unique_ptr<detail::ClientManager> client_manager_;
  std::unique_ptr<detail::platform::ServiceThread> worker_;
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

  impl_->bus_->start();
  impl_->bus_handler_->start();
  impl_->scheduler_->start();
  impl_->client_manager_->start();

  // Trigger initial system discovery if enabled
  std::lock_guard<std::mutex> lock(config_mutex_);
  if (config_.runtime.system_inquiry) triggerInquiryOfExistence();

  impl_->worker_ = std::make_unique<detail::platform::ServiceThread>(
      "ebusController", [this] { run(); },
      detail::OrchestrationLimits::stack_size,
      detail::OrchestrationLimits::priority_low, 0);
  impl_->worker_->start();

  return true;
}

void Controller::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) return;

  if (impl_->worker_) impl_->worker_->join();
  impl_->client_manager_->stop();
  impl_->scheduler_->stop();
  impl_->bus_handler_->stop();
  impl_->bus_->stop();
}

bool Controller::isRunning() const noexcept { return running_.load(); }

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
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_ = config;
  constructMembers();
  configured_.store(true);
  return true;
}

bool Controller::isConfigured() const noexcept { return configured_.load(); }

std::string Controller::getConfigJson() const {
  std::lock_guard<std::mutex> lock(config_mutex_);
  return toJson(config_);
}

void Controller::setAddress(const uint8_t& address) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.address = address;
  if (isConfigured()) {
    impl_->handler_->setSourceAddress(address);
    impl_->device_manager_->setOwnAddress(address);
    impl_->device_scanner_->setOwnAddress(address);
    impl_->poll_manager_->setOwnAddress(address);
    impl_->bus_->setRuntimeConfig(config_.runtime);
    impl_->request_->setLockCounter(config_.runtime.lock_counter);
  }
}

void Controller::setLockCounter(const uint8_t& lock_counter) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.lock_counter = lock_counter;
  if (isConfigured()) {
    impl_->request_->setLockCounter(lock_counter);
  }
}

void Controller::setSystemInquiry(bool enable) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.system_inquiry = enable;
}

void Controller::setSystemResponse(bool enable) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.system_response = enable;
}

void Controller::setWindow(const uint16_t& window_us) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.bus.window_us = window_us;
  if (isConfigured()) impl_->bus_->setWindow(window_us);
}

void Controller::setOffset(const uint16_t& offset_us) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.bus.offset_us = offset_us;
  if (isConfigured()) impl_->bus_->setOffset(offset_us);
}

void Controller::setWatchdogTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.bus.watchdog_timeout_ms = timeout_ms;
  if (isConfigured()) {
    impl_->bus_handler_->setWatchdogTimeout(timeout_ms);
  }
}

void Controller::setLogLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.logging.level = level;
  // Log level is checked dynamically in Scheduler callbacks
}

void Controller::setErrorLogSize(size_t size) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.logging.log_size = size;
  if (isConfigured()) impl_->error_buffer_.set_capacity(size);
}

void Controller::setSessionTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.network.session_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->client_manager_->setSessionTimeout(timeout_ms);
}

void Controller::setTransmitTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.network.transmit_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->client_manager_->setTransmitTimeout(timeout_ms);
}

void Controller::setOutboundBufferSize(size_t size) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.network.outbound_buffer_size = size;
  if (isConfigured()) {
    impl_->client_manager_->setOutboundBufferSize(size);
  }
}

void Controller::setScanOnStartup(bool enable) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scanner.scan_on_startup = enable;
  if (isConfigured()) impl_->device_scanner_->setScanOnStartup(enable);
}

void Controller::setMaxStartupScans(uint8_t max_scans) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scanner.max_startup_scans = max_scans;
  if (isConfigured()) {
    impl_->device_scanner_->setMaxStartupScans(max_scans);
  }
}

void Controller::setInitialScanDelay(uint32_t delay_s) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scanner.initial_delay_s = delay_s;
  if (isConfigured()) {
    impl_->device_scanner_->setInitialScanDelay(delay_s);
  }
}

void Controller::setStartupScanInterval(uint32_t interval_s) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scanner.startup_interval_s = interval_s;
  if (isConfigured()) {
    impl_->device_scanner_->setStartupScanInterval(interval_s);
  }
}

void Controller::setMaxSendAttempts(uint8_t max_send_attempts) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scheduler.max_send_attempts = max_send_attempts;
  if (isConfigured()) {
    impl_->scheduler_->setMaxSendAttempts(max_send_attempts);
  }
}

void Controller::setBaseBackoff(uint32_t base_backoff_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scheduler.base_backoff_ms = base_backoff_ms;
  if (isConfigured()) impl_->scheduler_->setBaseBackoff(base_backoff_ms);
}

void Controller::setFsmTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scheduler.fsm_timeout_ms = timeout_ms;
  if (isConfigured()) {
    impl_->scheduler_->setFsmTimeout(timeout_ms);
  }
}

void Controller::setTotalTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  config_.runtime.scheduler.total_timeout_ms = timeout_ms;
  if (isConfigured()) impl_->scheduler_->setTotalTimeout(timeout_ms);
}

void Controller::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  impl_->user_reactive_callback_ = std::move(callback);
  if (isConfigured())
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
}

void Controller::setTelegramCallback(TelegramCallback callback) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  impl_->user_telegram_callback_ = std::move(callback);
}

void Controller::setErrorCallback(ErrorCallback callback) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  impl_->user_error_callback_ = std::move(callback);
}

void Controller::setTraceCallback(TraceCallback callback) {
  std::lock_guard<std::mutex> lock(config_mutex_);
  impl_->user_trace_callback_ = std::move(callback);
}

bool Controller::enqueue(uint8_t priority, ByteView message,
                         ResultCallback callback) {
  if (isConfigured())
    return impl_->scheduler_->enqueue(priority, message, std::move(callback));

  return false;
}

bool Controller::enqueueAt(uint8_t priority, ByteView message,
                           Clock::time_point when, ResultCallback callback) {
  if (isConfigured())
    return impl_->scheduler_->enqueueAt(priority, message, when,
                                        std::move(callback));
  return false;
}

uint32_t Controller::addPollItem(uint8_t priority, ByteView message,
                                 uint32_t interval_ms,
                                 ResultCallback callback) {
  uint32_t id = isConfigured()
                    ? impl_->poll_manager_->addPollItem(
                          priority, message, interval_ms, std::move(callback))
                    : 0;
  wake_cv_.notify_one();
  return id;
}

void Controller::removePollItem(uint32_t id) {
  if (isConfigured()) impl_->poll_manager_->removePollItem(id);
}

void Controller::clearPollItems() {
  if (isConfigured()) impl_->poll_manager_->clear();
}

void Controller::triggerInquiryOfExistence() {
  // Standard eBUS System Discovery: Broadcast "Inquiry of Existence" (07h FEh)
  // This advises other masters that a new participant has entered the bus.
  enqueue(detail::ScannerLimits::scan_priority,
          ebus::Sequence::InquiryOfExistence());
}

void Controller::initFullScan(bool enable) {
  if (isConfigured()) impl_->device_scanner_->initFullScan(enable);
}

bool Controller::scanAddress(uint8_t address) {
  if (isConfigured()) {
    bool ret = impl_->device_scanner_->scanAddress(address);
    if (ret) wake_cv_.notify_one();
    return ret;
  }
  return false;
}

bool Controller::scanAddresses(const std::vector<uint8_t>& addresses) {
  if (isConfigured()) {
    bool ret = impl_->device_scanner_->scanAddresses(addresses);
    if (ret) wake_cv_.notify_one();
    return ret;
  }
  return false;
}

bool Controller::scanObservedDevices() {
  return isConfigured() ? impl_->device_scanner_->scanObservedDevices() : false;
}

bool Controller::isScanning() const {
  return isConfigured() ? impl_->device_scanner_->isScanning() : false;
}

void Controller::addClient(int fd, ClientType type) {
  if (isConfigured()) impl_->client_manager_->addClient(fd, type);
}

void Controller::removeClient(int fd) {
  if (isConfigured()) impl_->client_manager_->removeClient(fd);
}

std::vector<DeviceInfo> Controller::getDeviceInfo() const {
  if (!isConfigured()) return {};
  return impl_->device_manager_->getDeviceInfo();
}

std::string Controller::getDeviceInfoJson() const {
  if (!isConfigured()) return "[]";
  return impl_->device_manager_->getDeviceInfoJson();
}

void Controller::resetMetrics() {
  if (isConfigured()) impl_->bus_monitor_->resetMetrics();
}

Metrics Controller::getMetrics() const {
  return isConfigured() ? impl_->bus_monitor_->getMetrics() : Metrics{};
}

std::vector<float> Controller::getUtilizationHistory() const {
  if (isConfigured()) return impl_->bus_monitor_->getUtilizationHistory();
  return {};
}

std::vector<BusEventContext> Controller::getTraceHistory() const {
  if (isConfigured()) {
    return impl_->trace_buffer_.snapshot();
  }
  return {};
}

std::string Controller::getTraceHistoryJson() const {
  if (!isConfigured()) return "[]";
  return toJson(getTraceHistory());
}

std::vector<ErrorEntry> Controller::getErrors() const {
  return impl_->error_buffer_.snapshot();
}

std::string Controller::getErrorsJson() const { return toJson(getErrors()); }

size_t Controller::getErrorLogCapacity() const {
  return impl_->error_buffer_.capacity();
}

void Controller::clearErrors() { impl_->error_buffer_.clear(); }

void Controller::constructMembers() {
  // -- 1. Telemetry & Core Arbitration --
  if (!impl_->bus_monitor_) {
    impl_->bus_monitor_ = std::make_unique<detail::BusMonitor>();
  }

  if (!impl_->request_) {
    impl_->request_ =
        std::make_unique<detail::Request>(impl_->bus_monitor_.get());
  }
  impl_->request_->setLockCounter(config_.runtime.lock_counter);

  // -- 2. Physical Layer --
  // Note: configure() ensures we don't change hardware params while running
  if (!impl_->bus_) {
    impl_->bus_ = std::make_unique<detail::platform::Bus>(
        config_.bus, config_.runtime, impl_->request_.get(),
        impl_->bus_monitor_.get());
  } else {
    impl_->bus_->setRuntimeConfig(config_.runtime);
  }

  // -- 3. Protocol Handler --
  if (!impl_->handler_) {
    impl_->handler_ = std::make_unique<detail::Handler>(
        config_.runtime.address, impl_->bus_.get(), impl_->request_.get(),
        impl_->bus_monitor_.get());
  } else {
    impl_->handler_->setSourceAddress(config_.runtime.address);
  }

  // -- 4. Scheduler --
  if (!impl_->scheduler_) {
    impl_->scheduler_ =
        std::make_unique<detail::Scheduler>(impl_->handler_.get());

    // Setup internal event dispatchers
    impl_->scheduler_->setTelegramCallback([this](const TelegramInfo& info) {
      TelegramCallback user_callback;
      bool response_enabled = false;
      uint8_t own_address = 0xff;
      {
        std::lock_guard<std::mutex> lock(config_mutex_);
        user_callback = impl_->user_telegram_callback_;
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
            impl_->scheduler_->enqueue(detail::ScannerLimits::scan_priority,
                                       ebus::Sequence::SignOfLife());
          }
        }
      }

      if (user_callback) user_callback(info);
    });

    impl_->scheduler_->setErrorCallback([this](const ErrorInfo& info) {
      size_t current_log_size;
      LogLevel current_log_level;
      ErrorCallback user_callback;
      {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_log_size = config_.runtime.logging.log_size;
        current_log_level = config_.runtime.logging.level;
        user_callback = impl_->user_error_callback_;
      }
      if (current_log_size > 0) {
        ErrorEntry entry;
        entry.level = info.level;  // LogLevel is still used for filtering
        entry.setProtocolError(info.protocol_error);
        entry.result = info.result;
        entry.sequence_state = info.sequence_state;
        entry.handler_state = info.handler_state;
        entry.request_state = info.request_state;
        entry.setMaster(info.master_view.data(), info.master_view.size());
        entry.setSlave(info.slave_view.data(), info.slave_view.size());
        entry.utilization = info.utilization;
        entry.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        impl_->error_buffer_.push_back(std::move(entry));
      }
      if (user_callback && info.level <= current_log_level) {
        user_callback(info);
      }
    });
  }

  // Update scheduler settings in-place
  impl_->scheduler_->setMaxSendAttempts(
      config_.runtime.scheduler.max_send_attempts);
  impl_->scheduler_->setBaseBackoff(config_.runtime.scheduler.base_backoff_ms);
  impl_->scheduler_->setFsmTimeout(config_.runtime.scheduler.fsm_timeout_ms);
  impl_->scheduler_->setTotalTimeout(
      config_.runtime.scheduler.total_timeout_ms);
  if (impl_->user_reactive_callback_) {
    impl_->scheduler_->setReactiveMasterSlaveCallback(
        impl_->user_reactive_callback_);
  }

  // -- 5. Application Logic --
  if (!impl_->device_manager_) {
    impl_->device_manager_ =
        std::make_unique<detail::DeviceManager>(impl_->bus_monitor_.get());
  }
  impl_->device_manager_->setOwnAddress(config_.runtime.address);

  if (!impl_->device_scanner_) {
    impl_->device_scanner_ = std::make_unique<detail::DeviceScanner>(
        config_.runtime.address, impl_->device_manager_.get());
  }
  impl_->device_scanner_->setOwnAddress(config_.runtime.address);
  impl_->device_scanner_->setScanOnStartup(
      config_.runtime.scanner.scan_on_startup);
  impl_->device_scanner_->setInitialScanDelay(
      config_.runtime.scanner.initial_delay_s);
  impl_->device_scanner_->setStartupScanInterval(
      config_.runtime.scanner.startup_interval_s);
  impl_->device_scanner_->setMaxStartupScans(
      config_.runtime.scanner.max_startup_scans);

  if (!impl_->poll_manager_) {
    impl_->poll_manager_ = std::make_unique<detail::PollManager>();
  }
  impl_->poll_manager_->setOwnAddress(config_.runtime.address);

  // -- 6. Plumbing --
  if (!impl_->bus_handler_) {
    impl_->bus_handler_ = std::make_unique<detail::BusHandler>(
        impl_->request_.get(), impl_->handler_.get(), impl_->bus_->getQueue(),
        detail::BusLimits::max_listeners);

    // Add the permanent tracing listener
    impl_->bus_handler_->addByteListener([this](const BusEventContext& ctx) {
      // Store in internal history
      impl_->trace_buffer_.push_back(BusEventContext(ctx));

      // Invoke user callback if registered
      TraceCallback user_callback;
      {
        std::lock_guard<std::mutex> lock(config_mutex_);
        user_callback = impl_->user_trace_callback_;
      }
      if (user_callback) user_callback(ctx);
    });
  }
  impl_->bus_handler_->setWatchdogTimeout(
      config_.runtime.bus.watchdog_timeout_ms);

  if (!impl_->client_manager_) {
    impl_->client_manager_ = std::make_unique<detail::ClientManager>(
        impl_->bus_.get(), impl_->bus_handler_.get(), impl_->request_.get(),
        impl_->bus_monitor_.get());
  }  // Update client manager settings in-place
  impl_->client_manager_->setSessionTimeout(
      config_.runtime.network.session_timeout_ms);
  impl_->client_manager_->setTransmitTimeout(
      config_.runtime.network.transmit_timeout_ms);
  impl_->client_manager_->setOutboundBufferSize(
      config_.runtime.network.outbound_buffer_size);

  impl_->bus_->setWindow(config_.runtime.bus.window_us);
  impl_->bus_->setOffset(config_.runtime.bus.offset_us);
}

void Controller::run() {
  while (running_.load()) {
    bool activity = false;
    impl_->poll_manager_->processDueItems(
        [this, &activity](const detail::PollItem& item) {
          if (impl_->scheduler_->enqueue(item.priority, item.message,
                                         item.callback))
            activity = true;
        },
        &activity);

    if (impl_->scheduler_->queueSize() <
        detail::SchedulerLimits::scan_threshold) {
      auto scan_cmd = impl_->device_scanner_->nextCommand();
      if (!scan_cmd.empty()) {
        impl_->scheduler_->enqueue(
            detail::ScannerLimits::scan_priority, scan_cmd,
            [this](const ebus::ResultInfo& info) {
              if (!info.success &&
                  (info.result == RequestResult::first_lost ||
                   info.result == RequestResult::second_lost)) {
                // Re-enqueue address for scan if we lost arbitration.
                // Note: We whiltelist arbitration loss to avoid re-probing on
                // structural protocol errors (SequenceState) which would be
                // futile. Noise (first_error) is handled by the Scheduler's
                // own retry logic.
                if (info.master_view.size() > 1) {
                  impl_->device_scanner_->scanAddress(info.master_view[1]);
                }
              }
            });
        activity = true;
      }
    }

    std::unique_lock<std::mutex> lk(wake_mutex_);
    wake_cv_.wait_for(
        lk,
        std::chrono::milliseconds(detail::SchedulerLimits::controller_tick_ms),
        [this, activity] { return !running_.load() || activity; });
  }
}

}  // namespace ebus
