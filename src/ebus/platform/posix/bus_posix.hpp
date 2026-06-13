/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(POSIX) && !EBUS_SIMULATION
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ebus/config.hpp>
#include <ebus/status.hpp>
#include <ebus/types.hpp>
#include <functional>
#include <memory>

#include "core/bus_events.hpp"
#include "platform/bus_base.hpp"
#include "platform/mutex.hpp"
#include "platform/service_thread.hpp"

namespace ebus::detail {
class Request;
class BusMonitor;
}  // namespace ebus::detail

namespace ebus::detail::platform {

/**
 * POSIX-specific implementation of the eBUS physical layer.
 * Handles serial port configuration and asynchronous byte reading via a
 * background thread. Supports a virtual-line simulation for testing.
 */
class BusPosix : public BusBase {
 public:
  // Lifecycle
  BusPosix(const BusConfig& config, const ebus::RuntimeConfig& runtime,
           detail::Request* request, detail::BusMonitor* monitor = nullptr);
  ~BusPosix();
  void start();
  void stop();

  // Special Members & Operators
  BusPosix(const BusPosix&) = delete;
  BusPosix& operator=(const BusPosix&) = delete;

  // Configuration
  // kept for BusEsp compatibility, but not used in Posix implementation
  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  // Working Methods
  void writeByte(const uint8_t byte);

  // Status/Telemetry
  platform::ServiceThread::Status getThreadStatus() const;
  platform::ServiceThread::Status getSynThreadStatus() const;
  ebus::BusStatus fetchStatus() const;

 private:
  BusConfig config_;
  RuntimeConfig runtime_;

  detail::Request* request_ = nullptr;
  detail::BusMonitor* monitor_ = nullptr;

  int fd_;

  bool open_;
  struct termios old_settings_{};

  std::unique_ptr<ServiceThread> worker_;
  std::atomic<bool> running_;

  // SYN generator members
  std::unique_ptr<ServiceThread> syn_worker_;
  std::atomic<bool> syn_running_{false};
  platform::Mutex syn_mutex_;
  platform::ConditionVariable syn_cv_;

  std::chrono::milliseconds syn_base_ms_dur_;
  std::chrono::milliseconds syn_tolerance_ms_dur_;
  std::chrono::milliseconds current_t_unique_;

  Clock::time_point last_activity_time_;
  Clock::time_point next_syn_expiry_;
  Clock::time_point syn_intent_time_;
  bool syn_active_{false};

  std::atomic<bool> bus_request_flag_{false};

  void recordUtilization(uint8_t byte);

  void ensureOpen() const;

  // Thread function: read bytes and push them into the queue
  void readerThread();

  // SYN generator (optional)
  void synThread();

  // called when a symbol (end-of-byte) is recognised
  void resetSynTimer(uint8_t byte);
  void resetSynTimerInternal(uint8_t byte);
};

}  // namespace ebus::detail::platform

#endif  // POSIX