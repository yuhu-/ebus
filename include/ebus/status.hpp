/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <bitset>
#include <cstddef>
#include <string>

#include "ebus/static_vector.hpp"
#include "ebus/types.hpp"

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
  ThreadStatus() = default;
  ThreadStatus(std::string_view n, int32_t stack_bytes,
               int32_t stack_free_bytes)
      : name(n), stack_size(stack_bytes), stack_free(stack_free_bytes) {}
  FixedString<24> name;
  int32_t stack_size = -1;
  int32_t stack_free = -1;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of a system queue's health.
 */
struct QueueStatus {
  QueueStatus() = default;
  QueueStatus(std::string_view n, size_t s, size_t cap, size_t max_s)
      : name(n), size(s), capacity(cap), max_size(max_s) {}
  FixedString<24> name;
  size_t size = 0;
  size_t capacity = 0;
  size_t max_size = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the reactor's current state.
 */
struct ReactorStatus {
  ReactorStatus() = default;
  ReactorStatus(ThreadStatus t, QueueStatus sq, QueueStatus pq, QueueStatus bq,
                uint32_t sq_dropped, uint32_t pq_dropped, uint32_t bq_dropped,
                uint32_t loop_us)
      : thread(std::move(t)),
        signal_queue(std::move(sq)),
        protocol_queue(std::move(pq)),
        bus_queue(std::move(bq)),
        signal_queue_dropped(sq_dropped),
        protocol_queue_dropped(pq_dropped),
        bus_queue_dropped(bq_dropped),
        max_loop_cycle_us(loop_us) {}
  ThreadStatus thread;
  QueueStatus signal_queue;
  QueueStatus protocol_queue;
  QueueStatus bus_queue;
  uint32_t signal_queue_dropped = 0;
  uint32_t protocol_queue_dropped = 0;
  uint32_t bus_queue_dropped = 0;
  uint32_t max_loop_cycle_us = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the bus layer's current state.
 */
struct BusStatus {
  BusStatus() = default;
  BusStatus(ThreadStatus bus, ThreadStatus syn)
      : bus_thread(std::move(bus)), syn_thread(std::move(syn)) {}
  ThreadStatus bus_thread;
  ThreadStatus syn_thread;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the bus handler's current state.
 */
struct BusHandlerStatus {
  BusHandlerStatus() = default;
  BusHandlerStatus(HandlerState hs, RequestState rs)
      : handler_state(hs), request_state(rs) {}
  HandlerState handler_state = HandlerState::passive_receive_master;
  RequestState request_state = RequestState::observe;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the scheduler's current state.
 */
struct SchedulerStatus {
  SchedulerStatus() = default;
  explicit SchedulerStatus(QueueStatus q) : queue(std::move(q)) {}
  QueueStatus queue;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Detailed information about a connected network client.
 */
struct ClientInfo {
  ClientInfo() = default;
  ClientInfo(int f, std::string_view t, bool conn, bool write, size_t buf_usage)
      : fd(f),
        type(t),
        connected(conn),
        write_capable(write),
        outbound_buffer_usage(buf_usage) {}
  int fd = -1;
  FixedString<12> type;
  bool connected = false;
  bool write_capable = false;
  size_t outbound_buffer_usage = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the client manager's current state.
 */
struct ClientManagerStatus {
  ClientManagerStatus() = default;
  ClientManagerStatus(ThreadStatus t, bool active, std::string_view state,
                      std::string_view err)
      : thread(std::move(t)),
        session_active(active),
        session_state(state),
        last_error(err) {}
  ThreadStatus thread;
  bool session_active = false;
  FixedString<12> session_state;
  FixedString<48> last_error;
  StaticVector<ClientInfo, detail::NetworkLimits::max_clients> clients;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the device manager's current state.
 */
struct DeviceManagerStatus {
  size_t identified_count = 0;
  size_t unknown_count = 0;
  size_t device_capacity = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the device scanner's current state.
 */
struct DeviceScannerStatus {
  bool is_scanning = false;
  bool full_scan_active = false;
  uint16_t full_scan_address = 0;
  bool scan_on_startup_enabled = false;
  bool startup_iteration_active = false;
  uint16_t startup_current_device_addr = 256;
  uint8_t startup_scan_count = 0;
  size_t pending_deep_scans = 0;
  size_t failed_scans = 0;
  size_t quarantined_scans = 0;
  uint32_t failure_resets = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Snapshot of the poll manager's current state.
 */
struct PollManagerStatus {
  PollManagerStatus() = default;
  PollManagerStatus(size_t count, size_t max_count, size_t capacity)
      : item_count(count), max_item_count(max_count), poll_capacity(capacity) {}
  size_t item_count = 0;
  size_t max_item_count = 0;
  size_t poll_capacity = 0;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Minimal snapshot of system resources (stacks and queues).
 */
struct SystemResources {
  uint64_t last_update_timestamp_ms = 0;
  bool is_configured = false;
  bool is_running = false;
  StaticVector<ThreadStatus, 8> threads;
  StaticVector<QueueStatus, 16> queues;

  void toJson(detail::JsonWriter& writer) const;
};

/**
 * Aggregated health and operational status of all internal services.
 */
struct ServiceStatus {
  uint64_t last_update_timestamp_ms = 0;
  ReactorStatus reactor;
  BusStatus bus;
  BusHandlerStatus bus_handler;
  SchedulerStatus scheduler;
  ClientManagerStatus client_manager;
  DeviceManagerStatus device_manager;
  DeviceScannerStatus device_scanner;
  PollManagerStatus poll_manager;

  void toJson(detail::JsonWriter& writer) const;
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
                            bool pretty = false);

}  // namespace ebus
