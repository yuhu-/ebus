/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Public headers required for the API signatures
#include "ebus/callbacks.hpp"
#include "ebus/config.hpp"
#include "ebus/device.hpp"
#include "ebus/metrics.hpp"
#include "ebus/sequence.hpp"
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
  void setWindow(const uint16_t& window);
  void setOffset(const uint16_t& offset);
  void setErrorLogSize(size_t size);
  void setMaxSendAttempts(int max_send_attempts);
  void setBaseBackoff(std::chrono::milliseconds base_backoff);
  void setFsmTimeout(std::chrono::milliseconds timeout);
  void setWatchdogTimeout(std::chrono::milliseconds timeout);
  void setClientActiveTimeout(std::chrono::milliseconds timeout);
  void setOutboundBufferSize(size_t size);

  // Unified Bus Listeners
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  // Scheduling & Polling
  void enqueue(uint8_t priority, ByteView message,
               ResultCallback callback = nullptr);

  uint32_t addPollItem(uint8_t priority, ByteView message,
                       std::chrono::milliseconds interval,
                       std::function<void(ByteView)> callback = nullptr);
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

  bool configured_ = false;
  bool running_ = false;

  void constructMembers();
  void run();
};

}  // namespace ebus