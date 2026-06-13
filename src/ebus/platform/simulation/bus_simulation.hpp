/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if EBUS_SIMULATION
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/status.hpp>
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
 * Simulation implementation of the eBUS physical layer.
 * Provides a virtual bus for testing and development without hardware.
 */
class BusSimulation : public BusBase {
 public:
  // Lifecycle
  explicit BusSimulation(const BusConfig& config, const RuntimeConfig& runtime,
                         detail::Request* request, detail::BusMonitor* monitor);
  ~BusSimulation();
  void start();
  void stop();

  // Special Members & Operators
  BusSimulation(const BusSimulation&) = delete;
  BusSimulation& operator=(const BusSimulation&) = delete;

  // Configuration
  void setWindow(const uint16_t window_us);
  void setOffset(const uint16_t offset_us);
  void setRuntimeConfig(const RuntimeConfig& runtime);

  // Working Methods
  void writeByte(const uint8_t byte);
  void recordUtilization(uint8_t byte);

  // Status/Telemetry
  platform::ServiceThread::Status getThreadStatus() const;
  platform::ServiceThread::Status getSynThreadStatus() const;
  ebus::BusStatus fetchStatus() const;

 private:
  BusConfig config_;
  RuntimeConfig runtime_;

  detail::Request* request_ = nullptr;
  detail::BusMonitor* monitor_ = nullptr;

  std::unique_ptr<ServiceThread> worker_;
  std::unique_ptr<ServiceThread> syn_worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> syn_running_{false};

  // Simulation SYN generator state
  platform::Mutex syn_mutex_;
  platform::ConditionVariable syn_cv_;
  Clock::time_point last_activity_time_;
  Clock::time_point next_syn_expiry_;
  Clock::time_point syn_intent_time_sim_;

  uint64_t syn_base_ms_ = 0;            // Base SYN interval in milliseconds
  uint64_t syn_tolerance_ms_ = 0;       // SYN tolerance in milliseconds
  uint64_t syn_address_factor_ms_ = 0;  // SYN address factor in milliseconds

  std::atomic<bool> bus_request_flag_{
      false};  // Flag to indicate a bus request is pending
  bool syn_active_{
      false};  // True if this instance is currently generating SYNs

  void simulationReaderLoop();
  void simulationSynLoop();
  void resetSynTimerSim(uint8_t byte);
  void resetSynTimerSimInternal(uint8_t byte);
};

}  // namespace ebus::detail::platform

#endif  // EBUS_SIMULATION