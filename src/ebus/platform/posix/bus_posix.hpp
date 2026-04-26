/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(POSIX)
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/types.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/bus_events.hpp"
#include "platform/posix/virtual_line.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

namespace ebus::detail {

class Request;
class BusMonitor;

/**
 * POSIX-specific implementation of the eBUS physical layer.
 * Handles serial port configuration and asynchronous byte reading via a
 * background thread. Supports a virtual-line simulation for testing.
 */
class BusPosix {
 public:
  using ReadListener = std::function<void(const uint8_t& byte)>;
  using WriteListener = std::function<void(const uint8_t& byte)>;
  using SynListener = std::function<void()>;

  BusPosix(const BusConfig& config, const ebus::RuntimeConfig& runtime,
           Request* request, BusMonitor* monitor = nullptr);
  ~BusPosix();

  BusPosix(const BusPosix&) = delete;
  BusPosix& operator=(const BusPosix&) = delete;

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  // kept for BusFreeRtos compatibility, but not used in Posix implementation
  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  // Listeners
  void addReadListener(ReadListener listener);
  void addWriteListener(WriteListener listener);
  void addSynListener(SynListener listener);

 private:
  std::string device_;
  bool simulate_;
  RuntimeConfig runtime_;

  Request* request_ = nullptr;
  BusMonitor* monitor_ = nullptr;

  int fd_;
  std::unique_ptr<VirtualLine> virtual_line_;

  bool open_;
  struct termios old_settings_{};

  std::unique_ptr<Queue<BusEvent>> byte_queue_;
  std::unique_ptr<ServiceThread> worker_;
  std::atomic<bool> running_;

  std::vector<ReadListener> read_listeners_;
  std::vector<WriteListener> write_listeners_;
  std::vector<SynListener> syn_listeners_;

  // SYN generator members
  std::unique_ptr<ServiceThread> syn_worker_;
  std::atomic<bool> syn_running_{false};
  std::mutex syn_mutex_;
  std::condition_variable syn_cv_;

  std::chrono::milliseconds syn_base_ms_dur_;
  std::chrono::milliseconds syn_tolerance_ms_dur_;
  std::chrono::milliseconds current_t_unique_;

  std::chrono::steady_clock::time_point last_activity_time_;
  std::chrono::steady_clock::time_point next_syn_expiry_;
  std::chrono::steady_clock::time_point syn_intent_time_;
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
};

}  // namespace ebus::detail

#endif  // POSIX