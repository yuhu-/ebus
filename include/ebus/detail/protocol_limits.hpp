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
inline constexpr double bit_time_us = 1000000.0 / 2400.0;  // ~416.67 us
inline constexpr double byte_center_bits = 9.5;     // midpoint of the stop bit
inline constexpr float start_bit_tolerance = 1.5f;  // bit times
}  // namespace Physical

// --- Component Layer ---
namespace FsmLimits {
inline constexpr std::size_t num_handler_states = 15;
inline constexpr size_t num_request_states = 4;
}  // namespace FsmLimits

// --- Orchestration Layer (Thread Priorities & Stacks) ---
namespace OrchestrationLimits {
inline constexpr size_t stack_size = 4096;
inline constexpr uint8_t priority_high = 15;
inline constexpr uint8_t priority_med = 10;
inline constexpr uint8_t priority_low = 5;
inline constexpr uint32_t termination_timeout_ms = 2000;
}  // namespace OrchestrationLimits

// --- Internal Engine Limits ---
namespace RequestLimits {
inline constexpr uint8_t lock_counter_max = 25;
}  // namespace RequestLimits

namespace BusLimits {
inline constexpr uint16_t window_min_us = 4000;
inline constexpr uint16_t window_max_us = 5000;
inline constexpr uint16_t offset_max_us = 500;

inline constexpr size_t queue_size = 256;
inline constexpr size_t event_queue_capacity = 16;
inline constexpr size_t max_listeners = 16;

namespace Syn {
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

namespace LoggingLimits {
inline constexpr size_t history_size = 60;
}  // namespace LoggingLimits

namespace NetworkLimits {
inline constexpr uint32_t transmit_timeout_ms = 500;
inline constexpr uint32_t wake_interval_ms = 20;
}  // namespace NetworkLimits

namespace ScannerLimits {
inline constexpr uint32_t initial_delay_s = 10;
inline constexpr uint32_t startup_interval_s = 60;
inline constexpr uint8_t max_startup_scans = 5;
inline constexpr uint8_t scan_priority = 5;
}  // namespace ScannerLimits

namespace SchedulerLimits {
inline constexpr size_t queue_reserve = 32;
inline constexpr size_t scan_threshold = 5;
inline constexpr uint32_t jitter_threshold_ms = 2;
inline constexpr uint32_t controller_tick_ms = 20;
}  // namespace SchedulerLimits

namespace SequenceLimits {
inline constexpr size_t default_capacity = 64;
inline constexpr uint8_t max_telegram_bytes = 48;
inline constexpr uint8_t max_data_bytes = 16;
}  // namespace SequenceLimits

}  // namespace ebus::detail