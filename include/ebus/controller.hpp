/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Public headers required for the API signatures
#include "ebus/callbacks.hpp"
#include "ebus/config.hpp"
#include "ebus/device.hpp"
#include "ebus/metrics.hpp"
#include "ebus/types.hpp"

namespace ebus {

struct Impl;

class Controller {
 public:
  Controller();
  explicit Controller(const EbusConfig& config);
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  void configure(const EbusConfig& config);
  void start();
  void stop();

  void setAddress(const uint8_t& address);
  void setWindow(const uint16_t& window_us);
  void setOffset(const uint16_t& offset_us);
  void setErrorLogSize(size_t size);
  void setMaxSendAttempts(int max_send_attempts);
  void setBaseBackoff(uint32_t base_backoff_ms);
  void setFsmTimeout(uint32_t timeout_ms);
  void setInitialScanDelay(uint32_t delay_s);
  void setStartupScanInterval(uint32_t interval_s);
  void setMaxStartupScans(uint8_t max_scans);
  void setWatchdogTimeout(uint32_t timeout_ms);
  void setClientActiveTimeout(uint32_t timeout_ms);
  void setOutboundBufferSize(size_t size);

  // Unified Bus Listeners
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  // Scheduling & Polling
  bool enqueue(uint8_t priority, ByteView message,
               ResultCallback callback = nullptr);

  bool enqueueAt(uint8_t priority, ByteView message, Clock::time_point when,
                 ResultCallback callback = nullptr);

  uint32_t addPollItem(uint8_t priority, ByteView message, uint32_t interval_ms,
                       ResultCallback callback = nullptr);
  void removePollItem(uint32_t id);

  // Scanning
  void setFullScan(bool enable);
  void setScanOnStartup(bool enable);
  void scanAddress(uint8_t address);
  void scanObservedDevices();
  bool isScanning() const;

  // External Client Bridge (WiFi/TCP)
  void addClient(int fd, ClientType type);
  void removeClient(int fd);

  // Device & Network State
  std::vector<DeviceInfo> getDeviceInfo() const;
  std::string getDeviceInfoJson() const;

  // Bus Health Metrics
  void resetMetrics();
  ebus::Metrics getMetrics() const;
  std::vector<float> getUtilizationHistory() const;

  // Diagnostic Log
  std::vector<ErrorEntry> getErrors() const;
  std::string getErrorsJson() const;
  size_t getErrorLogCapacity() const;
  void clearErrors();

  bool isConfigured() const noexcept;
  bool isRunning() const noexcept;

 private:
  EbusConfig config_;
  std::unique_ptr<Impl> impl_;

  std::atomic<bool> configured_{false};
  std::atomic<bool> running_{false};

  mutable std::mutex config_mutex_;
  mutable std::mutex wake_mutex_;
  std::condition_variable wake_cv_;

  void constructMembers();
  void run();
};

}  // namespace ebus