/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ebus::detail {
class BusMonitor;
}

namespace ebus::detail {
class JsonWriter;  // Forward declaration
}

namespace ebus {

/**
 * Snapshot of a service thread's health.
 */
struct ThreadStatus {
  std::string name;
  int32_t task_stack_bytes = -1;
  int32_t task_stack_free_bytes = -1;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the controller's current state.
 */
struct ControllerStatus {
  ThreadStatus thread;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the bus layer's current state.
 */
struct BusStatus {
  ThreadStatus bus_thread;
  ThreadStatus syn_thread;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the bus handler's current state.
 */
struct BusHandlerStatus {
  ThreadStatus thread;
  size_t queue_size = 0;
  size_t queue_capacity = 0;
  size_t max_queue_size = 0;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the scheduler's current state.
 */
struct SchedulerStatus {
  ThreadStatus thread;
  size_t queue_size = 0;
  size_t queue_capacity = 0;
  size_t max_queue_size = 0;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the client manager's current state.
 */
struct ClientManagerStatus {
  ThreadStatus thread;
  size_t queue_size = 0;
  size_t queue_capacity = 0;
  size_t max_queue_size = 0;
  bool session_active = false;
  std::string session_state;
  std::string last_error;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the device manager's current state.
 */
struct DeviceManagerStatus {
  size_t identified_count = 0;
  size_t unknown_count = 0;
  std::bitset<256> masters{};
  std::bitset<256> slaves{};

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the device scanner's current state.
 */
struct DeviceScannerStatus {
  bool is_scanning = false;
  bool full_scan_active = false;
  uint16_t full_scan_address = 0;
  bool scan_on_startup_enabled = false;
  uint8_t startup_scan_count = 0;
  size_t manual_queue_size = 0;
  size_t startup_queue_size = 0;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Snapshot of the poll manager's current state.
 */
struct PollManagerStatus {
  size_t item_count = 0;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Minimal snapshot of system resources (stacks and queues).
 */
struct SystemResources {
  uint64_t timestamp_ms = 0;
  std::vector<ThreadStatus> threads;

  struct QueueInfo {
    std::string name;
    size_t size = 0;
    size_t capacity = 0;
    size_t max_size = 0;

    void toJson(const JsonChunkVisitor& visitor) const;
  };
  std::vector<QueueInfo> queues;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * Aggregated health and operational status of all internal services.
 */
struct ServiceStatus {
  uint64_t last_update_timestamp_ms = 0;
  ControllerStatus controller;
  BusStatus bus;
  BusHandlerStatus bus_handler;
  SchedulerStatus scheduler;
  ClientManagerStatus client_manager;
  DeviceManagerStatus device_manager;
  DeviceScannerStatus device_scanner;
  PollManagerStatus poll_manager;

  void toJson(const JsonChunkVisitor& visitor) const;
};

/**
 * @brief Serializes ServiceStatus to a visitor, optionally including history
 * from the BusMonitor.
 *
 * This method yields chunks of JSON to minimize memory spikes on the
 * stack/heap.
 *
 * @param visitor Callback to receive JSON chunks.
 * @param status The ServiceStatus snapshot.
 * @param monitor Optional pointer to the BusMonitor for history serialization.
 * @param reset_histories If true and monitor is provided, resets history
 * buffers after serialization.
 */
void serializeServiceStatus(const JsonChunkVisitor& visitor,
                            const ServiceStatus& status,
                            detail::BusMonitor* monitor = nullptr,
                            bool reset_histories = false);

}  // namespace ebus
