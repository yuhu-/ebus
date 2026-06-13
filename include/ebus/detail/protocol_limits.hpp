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
inline constexpr uint8_t bits_per_byte = 10;  // 1 start + 8 data + 1 stop
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
inline constexpr uint8_t controller_priority = 10;
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

// ESP Service Threads
inline constexpr uint32_t termination_timeout_ms = 2000;
}  // namespace OrchestrationLimits

// --- Internal Engine Limits ---
namespace RequestLimits {
inline constexpr uint8_t lock_counter_max = 25;
inline constexpr uint32_t collision_byte_count = 1;
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
inline constexpr int uart_install_retries = 3;
inline constexpr uint32_t uart_install_retry_delay_ms = 100;
inline constexpr uint32_t timer_resolution_hz = 1000000;  // 1us
inline constexpr uint32_t timer_intr_priority = 3;
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

/**
 * Factor used to drop data when a client buffer is full (e.g. 8 = drop 1/8th).
 */
inline constexpr size_t outbound_buffer_drop_factor = 8;
}  // namespace NetworkLimits

// --- Application Layer ---
namespace ControllerLimits {
#ifndef EBUS_EVENT_QUEUE_SIZE
inline constexpr size_t event_queue_size = 16;
#else
inline constexpr size_t event_queue_size = EBUS_EVENT_QUEUE_SIZE;
#endif

#ifndef EBUS_REACTOR_QUEUE_SIZE
inline constexpr size_t reactor_queue_size = 32;
#else
inline constexpr size_t reactor_queue_size = EBUS_REACTOR_QUEUE_SIZE;
#endif
inline constexpr uint32_t reactor_yield_burst_limit = 10;  // every 10th events
inline constexpr uint32_t latency_warning_threshold_us = 100000;
inline constexpr uint32_t status_update_interval_ms_fast = 100;
inline constexpr uint32_t status_update_interval_ms_slow = 500;
static_assert(reactor_queue_size >= 1, "Reactor queue size must be at least 1");
}  // namespace ControllerLimits

namespace DeviceLimits {
#ifndef EBUS_MAX_DEVICES
inline constexpr size_t max_devices = 32;
#else
inline constexpr size_t max_devices = EBUS_MAX_DEVICES;
#endif

inline constexpr uint8_t scan_priority = 5;

#ifndef EBUS_MAX_MANUAL_QUEUE
inline constexpr size_t max_manual_queue = 64;
#else
inline constexpr size_t max_manual_queue = EBUS_MAX_MANUAL_QUEUE;
#endif

#ifndef EBUS_MAX_STARTUP_QUEUE
inline constexpr size_t max_startup_queue = 64;
#else
inline constexpr size_t max_startup_queue = EBUS_MAX_STARTUP_QUEUE;
#endif
}  // namespace DeviceLimits

namespace SchedulerLimits {
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

// --- Formatting Limits ---
namespace FormattingLimits {
inline constexpr float float_lower_threshold = 1e-6f;
inline constexpr float float_upper_threshold = 1e10f;
inline constexpr size_t iso8601_buffer_size = 26;
inline constexpr int default_precision = 2;
inline constexpr int detailed_precision = 4;
}  // namespace FormattingLimits

// --- JSON Processing Limits ---
namespace JsonLimits {
inline constexpr size_t max_recursion_depth = 32;
inline constexpr int indent_spaces = 2;
inline constexpr size_t writer_buffer_size = 256;
inline constexpr size_t formatting_buffer_size = 64;
}  // namespace JsonLimits

// --- Logger Limits ---
namespace LoggerLimits {
inline constexpr size_t log_buffer_size = 256;
}  // namespace LoggerLimits

// --- System Metrics Limits ---
namespace SystemMetricsLimits {
inline constexpr size_t top_error_addresses_count = 3;
inline constexpr float bus_congestion_threshold_percent = 70.0f;
inline constexpr uint64_t bus_congestion_detection_time_us = 10000000;
inline constexpr uint32_t bus_congestion_detection_time_s = 10;
inline constexpr uint32_t bus_high_jitter_threshold_us = 10000;

// Quality Score Factors
inline constexpr float quality_score_drop_penalty = 0.7f;
inline constexpr float quality_score_jitter_penalty = 0.8f;
inline constexpr float quality_score_postpone_penalty = 0.9f;
inline constexpr float quality_score_start_bit_error_penalty = 0.9f;
}  // namespace SystemMetricsLimits

// --- Simulation Limits ---
namespace SimulationLimits {
inline constexpr size_t outbound_queue_capacity = 16;
}  // namespace SimulationLimits

namespace EnhancedProtocolLimits {
inline constexpr size_t max_sequence_len = 2;
inline constexpr uint8_t data_threshold = 0x80;
}  // namespace EnhancedProtocolLimits

// --- BCD Limits ---
namespace BcdLimits {
inline constexpr uint8_t nibble_mask = 0x0f;
inline constexpr uint8_t nibble_shift = 4;
inline constexpr uint8_t max_digit = 9;
inline constexpr uint8_t decimal_base = 10;
inline constexpr int64_t max_value = 99;
inline constexpr uint8_t null_sentinel = 0xff;
}  // namespace BcdLimits

// --- Data Type Sentinels (Replacement Values) ---
namespace DataTypeLimits {
inline constexpr uint8_t sentinel_8 = 0xff;
inline constexpr uint8_t sentinel_s8 = 0x80;  // -128
inline constexpr uint16_t sentinel_16 = 0xffff;
inline constexpr uint16_t sentinel_s16 = 0x8000;  // -32768
inline constexpr uint32_t sentinel_32 = 0xffffffff;
inline constexpr uint32_t sentinel_s32 = 0x80000000;  // -2147483648
}  // namespace DataTypeLimits

// --- Fixed-Point Scaling ---
namespace FixedPointLimits {
inline constexpr int64_t fixed_point_scale = 1000000LL;
inline constexpr uint8_t float_size = 4;
}  // namespace FixedPointLimits

}  // namespace ebus::detail