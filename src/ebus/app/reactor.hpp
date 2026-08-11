/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/callbacks.hpp>
#include <ebus/types.hpp>
#include <functional>
#include <memory>

#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/scheduler.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "utils/circular_buffer.hpp"

namespace ebus::detail {
class BusMonitor;

/**
 * Compact signal type for the reactor event queue.
 * Lightweight replacement for cross-thread wakeups.
 */
struct ReactorSignal {
  enum class Type : uint8_t {
    shutdown,
    bus_byte,
    protocol_ready,
    timer_wakeup,
    user_request,
    callback_ready
  } type;

  uint8_t byte_batch_count = 0;  // Only used for bus_byte
};

/**
 * The Reactor owns the unified event loop and decoupled queues for the
 * orchestration thread. It coordinates Scheduler, PollManager, DeviceScanner,
 * and BusHandler from a single thread, processing bus events and application
 * requests without heavy locking on every byte.
 * ClientManager runs on its own thread and is not part of the reactor.
 */
class Reactor {
 public:
  Reactor(uint8_t own_address, bool system_response, Scheduler* scheduler,
          PollManager* poll_manager, DeviceScanner* device_scanner,
          DeviceManager* device_manager, BusMonitor* bus_monitor);
  ~Reactor();

  void start();
  void stop();
  void run();

  bool pushSignal(ReactorSignal&& signal);
  bool pushProtocolEvent(ProtocolEvent&& event);

  // User callbacks (dispatched on reactor thread)
  void setProtocolCallback(ProtocolCallback callback);
  void setTraceCallback(TraceCallback callback);
  void setLogLevel(LogLevel level);

  void onBusEventInfo(const BusEventInfo& info);

  // Diagnostics access
  void fetchTraceHistory(
      std::function<void(const BusEventInfo&)> callback) const;
  void fetchTraceHistory(const JsonChunkVisitor& visitor, bool pretty) const;
  void fetchErrors(std::function<void(const ErrorEntry&)> callback) const;
  void fetchErrors(const JsonChunkVisitor& visitor, bool pretty) const;
  void clearErrors();
  size_t getErrorLogCapacity() const;

  // Queue diagnostics
  size_t signalQueueSize() const { return signal_queue_.size(); }
  size_t maxSignalQueueSize() const { return max_signal_queue_.load(); }
  size_t protocolQueueSize() const { return protocol_events_.size(); }
  size_t maxProtocolQueueSize() const { return max_protocol_events_.load(); }

  const platform::ServiceThread* worker() const { return worker_.get(); }

 private:
  // System response configuration (for sign-of-life on inquiry of existence)
  const uint8_t own_address_;
  const bool system_response_;

  Scheduler* scheduler_ = nullptr;
  PollManager* poll_manager_ = nullptr;
  DeviceScanner* device_scanner_ = nullptr;
  DeviceManager* device_manager_ = nullptr;
  BusMonitor* bus_monitor_ = nullptr;

  platform::Queue<ReactorSignal> signal_queue_;
  platform::Queue<ProtocolEvent> protocol_events_;
  platform::Queue<BusEventInfo> bus_events_;

  // Diagnostics buffers (owned by Reactor, written from bus thread via
  // callbacks)
  detail::CircularBuffer<BusEventInfo, DiagnosticsLimits::trace_history_size>
      trace_buffer_;
  detail::CircularBuffer<ErrorEntry, DiagnosticsLimits::log_history_size>
      error_buffer_;

  std::atomic<size_t> max_protocol_events_{0};
  std::atomic<size_t> max_signal_queue_{0};
  std::atomic<bool> running_{false};

  std::unique_ptr<platform::ServiceThread> worker_;

  // User callbacks
  ProtocolCallback user_protocol_callback_;
  TraceCallback user_trace_callback_;

  void processSignal(const ReactorSignal& signal);
  void processPublicEvents();
  void processBusByte();
  void processProtocolReady();
  void processTimerWakeup();
  void processUserRequest();

  void log(LogLevel level, std::string_view msg) const;
  void logError(std::string_view msg) const { log(LogLevel::error, msg); }
  void logInfo(std::string_view msg) const { log(LogLevel::info, msg); }
  void logDebug(std::string_view msg) const { log(LogLevel::debug, msg); }
};

}  // namespace ebus::detail
