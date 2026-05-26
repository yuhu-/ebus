/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Internal constants and protocol limits for the eBUS library.
 * These are renamed with a 'Limits' suffix to avoid collisions with
 * core class names like 'Bus', 'Scheduler', and 'Sequence'.
 * It follows a logical [Subject]_[Constraint]_[Unit] hierarchy
 */
namespace ebus::detail {

// --- Physical Layer ---
namespace Physical {
inline constexpr uint32_t baud_rate = 2400;
inline constexpr uint64_t bit_time_num = 1250;
inline constexpr uint64_t bit_time_den = 3;
inline constexpr float bit_time_us = 1000000.0f / 2400.0f;  // ~416.67 us
inline constexpr float byte_center_bits = 9.5;      // midpoint of the stop bit
inline constexpr float start_bit_tolerance = 1.5f;  // bit times
}  // namespace Physical

// --- Component Layer ---
namespace FsmLimits {
inline constexpr std::size_t num_handler_states = 15;
inline constexpr size_t num_request_states = 4;

#ifndef EBUS_TRANSITION_HISTORY_SIZE
inline constexpr size_t transition_history_size = 10;
#else
inline constexpr size_t transition_history_size = EBUS_TRANSITION_HISTORY_SIZE;
#endif
static_assert(transition_history_size >= 1,
              "FSM transition history size must be at least 1");
}  // namespace FsmLimits

// --- Orchestration Layer (Thread Priorities & Stacks) ---
namespace OrchestrationLimits {

inline constexpr size_t default_stack_size = 2048;
inline constexpr uint8_t default_priority = 5;

#ifndef EBUS_CONTROLLER_STACK_SIZE
inline constexpr size_t controller_stack_size = 4096;
#else
inline constexpr size_t controller_stack_size = EBUS_CONTROLLER_STACK_SIZE;
#endif

#ifndef EBUS_CONTROLLER_PRIORITY
inline constexpr uint8_t controller_priority = 5;
#else
inline constexpr uint8_t controller_priority = EBUS_CONTROLLER_PRIORITY;
#endif

#ifndef EBUS_BUS_STACK_SIZE
inline constexpr size_t bus_stack_size = 2048;
#else
inline constexpr size_t bus_stack_size = EBUS_BUS_STACK_SIZE;
#endif

#ifndef EBUS_BUS_PRIORITY
inline constexpr uint8_t bus_priority = 15;
#else
inline constexpr uint8_t bus_priority = EBUS_BUS_PRIORITY;
#endif

#ifndef EBUS_BUS_SYN_STACK_SIZE
inline constexpr size_t bus_syn_stack_size = 2048;
#else
inline constexpr size_t bus_syn_stack_size = EBUS_BUS_SYN_STACK_SIZE;
#endif

#ifndef EBUS_BUS_SYN_PRIORITY
inline constexpr uint8_t bus_syn_priority = 5;
#else
inline constexpr uint8_t bus_syn_priority = EBUS_BUS_SYN_PRIORITY;
#endif

#ifndef EBUS_BUS_HANDLER_STACK_SIZE
inline constexpr size_t bus_handler_stack_size = 3072;
#else
inline constexpr size_t bus_handler_stack_size = EBUS_BUS_HANDLER_STACK_SIZE;
#endif

#ifndef EBUS_BUS_HANDLER_PRIORITY
inline constexpr uint8_t bus_handler_priority = 10;
#else
inline constexpr uint8_t bus_handler_priority = EBUS_BUS_HANDLER_PRIORITY;
#endif

#ifndef EBUS_SCHEDULER_STACK_SIZE
inline constexpr size_t scheduler_stack_size = 3072;
#else
inline constexpr size_t scheduler_stack_size = EBUS_SCHEDULER_STACK_SIZE;
#endif

#ifndef EBUS_SCHEDULER_PRIORITY
inline constexpr uint8_t scheduler_priority = 10;
#else
inline constexpr uint8_t scheduler_priority = EBUS_SCHEDULER_PRIORITY;
#endif

#ifndef EBUS_CLIENT_MANAGER_STACK_SIZE
inline constexpr size_t client_manager_stack_size = 2048;
#else
inline constexpr size_t client_manager_stack_size =
    EBUS_CLIENT_MANAGER_STACK_SIZE;
#endif

#ifndef EBUS_CLIENT_MANAGER_PRIORITY
inline constexpr uint8_t client_manager_priority = 10;
#else
inline constexpr uint8_t client_manager_priority = EBUS_CLIENT_MANAGER_PRIORITY;
#endif

// ESP Service Threads
inline constexpr uint32_t termination_timeout_ms = 2000;
}  // namespace OrchestrationLimits

// --- Internal Engine Limits ---
namespace RequestLimits {
inline constexpr uint8_t lock_counter_max = 25;
}  // namespace RequestLimits

// --- Container Limits ---
namespace SequenceLimits {
/**
 * Default capacity for Small Buffer Optimization (SBO).
 * Set to 64 to accommodate byte-stuffing expansion of a 48-byte logical
 * telegram.
 */
inline constexpr size_t default_capacity = 64;
/**
 * Capacity for metadata storage in models (e.g. Device identification).
 * Sized to fit a logical 16-byte payload plus protocol overhead (NN, CRC).
 */
inline constexpr size_t model_capacity = 24;
/**
 * Maximum number of bytes in a logical eBUS telegram (excluding SYN, ACK, and
 * byte-stuffing).
 */
inline constexpr uint8_t max_telegram_bytes = 48;
/**
 * Maximum number of data bytes that can be hold in a eBUS telegram.
 */
inline constexpr uint8_t max_data_bytes = 16;
}  // namespace SequenceLimits

// --- Bus Layer ---
namespace BusLimits {
inline constexpr uint16_t window_min_us = 4000;
inline constexpr uint16_t window_max_us = 5000;
inline constexpr uint16_t offset_max_us = 500;

#ifndef EBUS_BUS_QUEUE_SIZE
inline constexpr size_t queue_size = 256;
#else
inline constexpr size_t queue_size = EBUS_BUS_QUEUE_SIZE;
#endif

#ifndef EBUS_MAX_LISTENERS
inline constexpr size_t max_listeners = 4;
#else
inline constexpr size_t max_listeners = EBUS_MAX_LISTENERS;
#endif

namespace Syn {
inline constexpr uint32_t base_ms = 50;
inline constexpr uint32_t tolerance_ms = 5;
inline constexpr uint32_t address_factor_ms = 10;
inline constexpr uint32_t postpone_ms = 2;
inline constexpr uint32_t carrier_sense_ms = 5;
inline constexpr uint32_t serialization_delay_ms = 4;
}  // namespace Syn

namespace platform::Esp {
inline constexpr uint32_t event_timeout_ms = 10;
inline constexpr uint8_t falling_edge_history = 5;
}  // namespace platform::Esp

namespace platform::Posix {
inline constexpr uint32_t request_delay_us = 200;
inline constexpr uint32_t virtual_read_timeout_ms = 10;
}  // namespace platform::Posix
}  // namespace BusLimits

// --- Diagnostics Layer ---
namespace DiagnosticsLimits {
#ifndef EBUS_LOG_HISTORY_SIZE
inline constexpr size_t log_history_size = 60;
#else
inline constexpr size_t log_history_size = EBUS_LOG_HISTORY_SIZE;
#endif
static_assert(log_history_size >= 1,
              "Diagnostic log history size must be at least 1");

#ifndef EBUS_TRACE_HISTORY_SIZE
inline constexpr size_t trace_history_size = 100;
#else
inline constexpr size_t trace_history_size = EBUS_TRACE_HISTORY_SIZE;
#endif
static_assert(trace_history_size >= 1,
              "Protocol trace history size must be at least 1");
}  // namespace DiagnosticsLimits

// --- Networking Layer ---
namespace NetworkLimits {
inline constexpr uint32_t wake_interval_ms = 20;

#ifndef EBUS_MAX_CLIENTS
inline constexpr size_t max_clients = 4;
#else
inline constexpr size_t max_clients = EBUS_MAX_CLIENTS;
#endif
}  // namespace NetworkLimits

// --- Application Layer ---
namespace ControllerLimits {
#ifndef EBUS_PUBLIC_QUEUE_SIZE
inline constexpr size_t public_queue_size = 8;
#else
inline constexpr size_t public_queue_size = EBUS_PUBLIC_QUEUE_SIZE;
#endif
}  // namespace ControllerLimits
namespace DeviceLimits {
#ifndef EBUS_MAX_DEVICES
inline constexpr size_t max_devices = 32;
#else
inline constexpr size_t max_devices = EBUS_MAX_DEVICES;
#endif

inline constexpr uint8_t scan_priority = 5;

#ifndef EBUS_MAX_MANUAL_QUEUE
inline constexpr size_t max_manual_queue = 16;
#else
inline constexpr size_t max_manual_queue = EBUS_MAX_MANUAL_QUEUE;
#endif
}  // namespace DeviceLimits

namespace SchedulerLimits {
inline constexpr size_t queue_reserve = 32;

#ifndef EBUS_SCHEDULER_MAX_ITEMS
inline constexpr size_t max_items = 32;
#else
inline constexpr size_t max_items = EBUS_SCHEDULER_MAX_ITEMS;
#endif

inline constexpr size_t scan_threshold = 5;
inline constexpr uint32_t jitter_threshold_ms = 2;
inline constexpr uint32_t controller_tick_ms = 20;
}  // namespace SchedulerLimits

namespace PollLimits {
#ifndef EBUS_POLL_MAX_ITEMS
inline constexpr size_t max_items = 128;
#else
inline constexpr size_t max_items = EBUS_POLL_MAX_ITEMS;
#endif
}  // namespace PollLimits

}  // namespace ebus::detail