/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/reactor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ebus/utils.hpp>

#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "platform/service_thread.hpp"
#include "utils/logger.hpp"

namespace ebus::detail {

Reactor::Reactor(uint8_t own_address, bool system_response,
                 Scheduler* scheduler, PollManager* poll_manager,
                 DeviceScanner* device_scanner, DeviceManager* device_manager,
                 BusMonitor* bus_monitor)
    : own_address_(own_address),
      system_response_(system_response),
      scheduler_(scheduler),
      poll_manager_(poll_manager),
      device_scanner_(device_scanner),
      device_manager_(device_manager),
      bus_monitor_(bus_monitor),
      signal_queue_(ReactorLimits::reactor_queue_size),
      protocol_events_(ReactorLimits::event_queue_size),
      bus_events_(ReactorLimits::reactor_queue_size) {}

Reactor::~Reactor() { stop(); }

void Reactor::stop() {
  logInfo("Stopping reactor.");

  ReactorSignal shutdown_sig;
  shutdown_sig.type = ReactorSignal::Type::shutdown;
  signal_queue_.tryPush(std::move(shutdown_sig));
  signal_queue_.shutdown();

  if (worker_) {
    worker_->join();
    worker_.reset();
  }

  running_.store(false, std::memory_order_release);
}

void Reactor::start() {
  worker_ = std::make_unique<platform::ServiceThread>(
      "ebus_reactor",
      detail::Delegate<void()>::bind<Reactor, &Reactor::run>(this),
      detail::OrchestrationLimits::reactor_stack_size,
      detail::OrchestrationLimits::reactor_priority);
  worker_->start();
}

void Reactor::setLogLevel(LogLevel level) {
  detail::Logger::getInstance().setLevel(level);
}

void Reactor::run() {
  running_.store(true, std::memory_order_release);
  logInfo("Reactor thread started.");

  ReactorSignal signal;
  auto last_status_update = Clock::now();
  uint32_t burst_count = 0;

  while (running_.load()) {
    auto loop_start = Clock::now();
    bool activity = false;

    // 1. Scheduler tick - processes due messages, timeouts, sends next message
    if (scheduler_->tick()) activity = true;

    // 2. Drain protocol events queue -> user callbacks + injectProtocolEvent
    processPublicEvents();

    // 3. Process due poll items
    poll_manager_->processDueItems(
        [this, &activity](const PollManager::Item& item) {
          if (scheduler_->enqueue(item.priority, item.message, item.poll_id))
            activity = true;
        },
        &activity);

    // 4. Process due scan commands if scheduler has capacity
    if (scheduler_->size() < SchedulerLimits::scan_threshold) {
      auto scan_cmd = device_scanner_->nextCommand();
      if (!scan_cmd.empty() &&
          scheduler_->enqueue(DeviceLimits::scan_priority, scan_cmd)) {
        activity = true;
      }
    }

    // 5. Calculate next wakeup time
    const auto next_sched = scheduler_->nextDueTime();
    const auto next_poll = poll_manager_->nextDueTime();
    const auto tick_limit =
        Clock::now() +
        std::chrono::milliseconds(SchedulerLimits::controller_tick_ms);
    const auto next_wakeup = std::min({next_sched, next_poll, tick_limit});

    const auto now = Clock::now();
    uint32_t timeout_ms = 0;

    // If activity happened, don't wait (poll the queue)
    if (!activity && next_wakeup > now) {
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          next_wakeup - now);
      timeout_ms = static_cast<uint32_t>(duration.count());
    }

    // 6. Block on signal queue
    if (signal_queue_.pop(signal, timeout_ms)) {
      processSignal(signal);
      activity = true;

      // Drain loop: process all pending signals before housekeeping
      while (signal_queue_.tryPop(signal)) {
        processSignal(signal);
        if (!running_.load()) return;

        // CPU Starvation Fix: If processing a large burst, yield
        if (++burst_count > ReactorLimits::reactor_yield_burst_limit) {
          burst_count = 0;
          break;
        }
      }
    } else {
      burst_count = 0;
    }

    // 7. Ensure public events processed even if no signal arrived
    if (activity || timeout_ms == 0) {
      processPublicEvents();
    }

    // 8. Update loop performance metrics
    auto loop_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                             Clock::now() - loop_start)
                             .count();
    if (loop_duration > ReactorLimits::latency_warning_threshold_us) {
      logInfo("Loop iteration latency warning: " +
              std::to_string(loop_duration) + " us. Possible starvation?");
    }

    bus_monitor_->updateController([loop_duration](auto& m) {
      if (loop_duration > m.max_loop_cycle_us)
        m.max_loop_cycle_us = static_cast<uint32_t>(loop_duration);
    });

    // 9. Throttle status updates
    auto time_since_update = Clock::now() - last_status_update;
    if ((!activity && time_since_update >
                          std::chrono::milliseconds(
                              ReactorLimits::status_update_interval_ms_fast)) ||
        (time_since_update >
         std::chrono::milliseconds(
             ReactorLimits::status_update_interval_ms_slow))) {
      bus_monitor_->updateUtilizationHistory();

      // Reset windowed metrics
      bus_monitor_->resetLoopCycle();
      bus_monitor_->resetMaxReactorQueueSize(signal_queue_.size());
      scheduler_->resetPeakMetrics();
      device_scanner_->resetPeakMetrics();
      poll_manager_->resetPeakMetrics();

      last_status_update = Clock::now();
    }
  }

  logInfo("Reactor thread stopped.");
}

bool Reactor::pushSignal(ReactorSignal&& signal) {
  if (!signal_queue_.tryPush(std::move(signal))) {
    if (signal_queue_.discard() > 0) {
      if (signal_queue_.tryPush(std::move(signal))) {
        ebus::updateMaxAtomic(max_signal_queue_, signal_queue_.size());
        return true;
      }
    }
    if (bus_monitor_) {
      bus_monitor_->updateController([](auto& m) { m.event_queue_dropped++; });
    }
    return false;
  }
  ebus::updateMaxAtomic(max_signal_queue_, signal_queue_.size());
  return true;
}

bool Reactor::pushProtocolEvent(ProtocolEvent&& event) {
  if (!protocol_events_.tryPush(std::move(event))) {
    // DRAIN: Make room for protocol events
    if (protocol_events_.discard() > 0) {
      if (protocol_events_.tryPush(std::move(event))) {
        ebus::updateMaxAtomic(max_protocol_events_, protocol_events_.size());

        // Signal reactor that callback is ready for dispatch
        ReactorSignal cb_sig;
        cb_sig.type = ReactorSignal::Type::callback_ready;
        signal_queue_.tryPush(std::move(cb_sig));
        return true;
      }
    }
    if (bus_monitor_) {
      bus_monitor_->updateController([](auto& m) { m.event_queue_dropped++; });
    }
    return false;
  }
  ebus::updateMaxAtomic(max_protocol_events_, protocol_events_.size());

  // Signal reactor that callback is ready for dispatch
  ReactorSignal cb_sig;
  cb_sig.type = ReactorSignal::Type::callback_ready;
  signal_queue_.tryPush(std::move(cb_sig));
  return true;
}

void Reactor::setProtocolCallback(ProtocolCallback callback) {
  user_protocol_callback_ = std::move(callback);
}

void Reactor::setTraceCallback(TraceCallback callback) {
  user_trace_callback_ = std::move(callback);
}

void Reactor::onBusEventInfo(const BusEventInfo& info) {
  if (!bus_events_.tryPush(info)) {
    if (bus_events_.discard() > 0) {
      bus_events_.tryPush(info);
    }
    if (bus_monitor_) {
      bus_monitor_->updateController([](auto& m) { m.event_queue_dropped++; });
    }
  }

  trace_buffer_.push_back(info);

  ReactorSignal sig;
  sig.type = ReactorSignal::Type::bus_byte;
  sig.byte_batch_count = 1;
  signal_queue_.tryPush(std::move(sig));
}

void Reactor::processSignal(const ReactorSignal& signal) {
  switch (signal.type) {
    case ReactorSignal::Type::shutdown:
      running_.store(false, std::memory_order_release);
      logDebug("Shutdown signal received");
      break;

    case ReactorSignal::Type::bus_byte:
      processBusByte();
      break;

    case ReactorSignal::Type::protocol_ready:
      processProtocolReady();
      break;

    case ReactorSignal::Type::timer_wakeup:
      processTimerWakeup();
      break;

    case ReactorSignal::Type::user_request:
      processUserRequest();
      break;

    case ReactorSignal::Type::callback_ready:
      // Just a wakeup - processPublicEvents handles the actual callbacks
      break;
  }
}

void Reactor::processBusByte() {
  BusEventInfo info;
  if (bus_events_.tryPop(info)) {
    TraceCallback user_callback = user_trace_callback_;
    if (user_callback) user_callback(info);
  }
}

void Reactor::processProtocolReady() {
  // Just a wakeup - processPublicEvents does the work
}

void Reactor::processTimerWakeup() {
  // Just a wakeup - tick() already processed due items
}

void Reactor::processUserRequest() {
  // Just a wakeup - enqueue() already scheduled the item
}

void Reactor::processPublicEvents() {
  ProtocolCallback user_callback = user_protocol_callback_;

  ProtocolEvent ev;
  while (protocol_events_.tryPop(ev)) {
    scheduler_->injectProtocolEvent(ev);

    if (ev.type == ProtocolEvent::Type::telegram) {
      if (device_manager_)
        device_manager_->update({ev.master.data(), ev.master.size()},
                                {ev.slave.data(), ev.slave.size()});

      if (device_scanner_ && ev.session_id > 0) {
        bool is_broadcast =
            (ev.type == ProtocolEvent::Type::telegram &&
             ev.data.tel.telegram_type == TelegramType::broadcast);
        if (!is_broadcast) device_scanner_->onScanResult(ev.master[1], true);
      }

      // System response: if we received an Inquiry of Existence (07 FE)
      // from another master, respond with Sign of Life (07 FF)
      if (system_response_) {
        // Inquiry of Existence (Service 07h FEh): PB=07, SB=FE, NN=00
        if (ebus::matches(ev.master, ebus::Sequence::inquiryOfExistence(), 1)) {
          if (ev.master[0] != own_address_) {
            scheduler_->enqueue(detail::DeviceLimits::scan_priority,
                                ebus::Sequence::signOfLife());
          }
        }
      }
    } else if (ev.type == ProtocolEvent::Type::error) {
      ErrorEntry entry;
      entry.session_id = ev.session_id;
      entry.poll_id = ev.poll_id;
      entry.level = ev.data.err.level;
      entry.protocol_error = ev.data.err.protocol_error;
      entry.result = ev.data.err.result;
      entry.sequence_state = ev.data.err.sequence_state;
      entry.handler_state = ev.handler_state;
      entry.request_state = ev.request_state;
      entry.retry_count = ev.retry_count;
      entry.setMaster(ev.master.data(), ev.master.size());
      entry.setSlave(ev.slave.data(), ev.slave.size());
      entry.timestamp = ebus::getWallTimeMs();
      error_buffer_.push_back(std::move(entry));

      if (device_scanner_ && ev.session_id > 0) {
        bool is_broadcast =
            (ev.type == ProtocolEvent::Type::telegram &&
             ev.data.tel.telegram_type == TelegramType::broadcast);
        if (!is_broadcast) device_scanner_->onScanResult(ev.master[1], false);
      }
    }

    if (user_callback) {
      // Only deliver telegram and error events to user callback.
      // Won/lost are internal arbitration results.
      if (ev.type == ProtocolEvent::Type::telegram ||
          ev.type == ProtocolEvent::Type::error) {
        ProtocolInfo info;
        info.is_error = (ev.type == ProtocolEvent::Type::error);
        info.session_id = ev.session_id;
        info.poll_id = ev.poll_id;
        info.retry_count = ev.retry_count;
        info.handler_state = ev.handler_state;
        info.request_state = ev.request_state;
        info.master_view = {ev.master.data(), ev.master.size()};
        info.slave_view = {ev.slave.data(), ev.slave.size()};

        if (info.is_error) {
          info.level = ev.data.err.level;
          info.protocol_error = ev.data.err.protocol_error;
          info.result = ev.data.err.result;
          info.sequence_state = ev.data.err.sequence_state;
          if (!detail::Logger::getInstance().isEnabled(info.level)) continue;
        } else {
          info.message_type = ev.data.tel.message_type;
          info.telegram_type = ev.data.tel.telegram_type;
        }

        user_callback(info);
      }
    }
  }
}

// Diagnostics accessors
void Reactor::fetchTraceHistory(
    std::function<void(const BusEventInfo&)> callback) const {
  if (callback) trace_buffer_.forEach(callback);
}

void Reactor::fetchTraceHistory(const JsonChunkVisitor& visitor,
                                bool pretty) const {
  if (visitor) {
    detail::JsonWriter writer(visitor, pretty);
    auto scope = writer.arrayScope();
    trace_buffer_.forEach(
        [&](const BusEventInfo& info) { writer.writeValue(info); });
  }
}

void Reactor::fetchErrors(
    std::function<void(const ErrorEntry&)> callback) const {
  if (callback) error_buffer_.forEach(callback);
}

void Reactor::fetchErrors(const JsonChunkVisitor& visitor, bool pretty) const {
  if (visitor) {
    detail::JsonWriter writer(visitor, pretty);
    auto scope = writer.arrayScope();
    error_buffer_.forEach(
        [&](const ErrorEntry& entry) { writer.writeValue(entry); });
  }
}

void Reactor::clearErrors() { error_buffer_.clear(); }

size_t Reactor::getErrorLogCapacity() const { return error_buffer_.capacity(); }

void Reactor::log(LogLevel level, std::string_view msg) const {
  auto& logger = detail::Logger::getInstance();
  if (!logger.isEnabled(level)) return;

  char buf[detail::LoggerLimits::log_buffer_size];
  int n = std::snprintf(buf, sizeof(buf), "[reactor] %.*s", (int)msg.size(),
                        msg.data());
  if (n > 0)
    logger.log(level,
               std::string_view(buf, std::min((size_t)n, sizeof(buf) - 1)));
}

}  // namespace ebus::detail