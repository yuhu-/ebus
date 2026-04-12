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
#include <ebus/config.hpp>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "core/request.hpp"
#include "platform/posix/virtual_line.hpp"
#include "platform/queue.hpp"
#include "utils/timing_stats.hpp"

namespace ebus {

struct BusEvent {
  uint8_t byte;
  bool busRequest{false};
  bool startBit{false};
  std::chrono::steady_clock::time_point timestamp;
};

#define EBUS_BUS_COUNTER_LIST X(startBit)

#define EBUS_BUS_TIMING_LIST \
  X(statsDelay)              \
  X(statsWindow)             \
  X(statsTransmit)           \
  X(statsUptime)

class BusPosix {
 public:
  using ReadListener = std::function<void(const uint8_t& byte)>;
  using WriteListener = std::function<void(const uint8_t& byte)>;
  using SynListener = std::function<void()>;

  BusPosix(const BusConfig& config, const RuntimeConfig& runtime,
           Request* request);
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

  void resetMetrics();
  std::map<std::string, MetricValues> getMetrics() const;
  void recordUtilization(uint8_t byte);

  // Listeners
  void addReadListener(ReadListener listener);
  void addWriteListener(WriteListener listener);
  void addSynListener(SynListener listener);

 private:
  std::string device_;
  bool simulate_;
  RuntimeConfig runtime_;

  Request* request_ = nullptr;

  int fd_;
  std::unique_ptr<VirtualLine> virtualLine_;

  bool open_;
  struct termios oldSettings_{};

  std::unique_ptr<Queue<BusEvent>> byteQueue_;
  std::thread thread_;
  std::atomic<bool> running_;

  std::vector<ReadListener> readListeners_;
  std::vector<WriteListener> writeListeners_;
  std::vector<SynListener> synListeners_;

  // SYN generator members
  std::thread synThread_;
  std::atomic<bool> synRunning_{false};
  std::mutex synMutex_;
  std::condition_variable synCv_;

  std::chrono::milliseconds synBaseMsDur_;
  std::chrono::milliseconds synToleranceMsDur_;
  std::chrono::milliseconds currentTunique_;

  std::chrono::steady_clock::time_point lastActivityTime_;
  std::chrono::steady_clock::time_point nextSynExpiry_;
  bool synActive_{false};

  std::atomic<bool> busRequestFlag_{false};

  // metrics
  struct Counter {
#define X(name) uint32_t name##_ = 0;
    EBUS_BUS_COUNTER_LIST
#undef X
  };

  Counter counter_;

  TimingStats statsDelay_;
  TimingStats statsWindow_;
  TimingStats statsTransmit_;
  RollingStats statsUtilization_;
  TimingStats statsUptime_;

  void ensureOpen() const;

  // Thread function: read bytes and push them into the queue
  void readerThread();

  // SYN generator (optional)
  void synThread();

  // called when a symbol (end-of-byte) is recognised
  void resetSynTimer(uint8_t byte);
};

}  // namespace ebus

#endif