/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace ebus {

// --- Protocol Symbols ---
constexpr uint8_t sym_zero = 0x00;     // zero byte
constexpr uint8_t sym_syn = 0xaa;      // synchronization byte
constexpr uint8_t sym_ext = 0xa9;      // extend byte
constexpr uint8_t sym_syn_ext = 0x01;  // extended synchronization byte
constexpr uint8_t sym_ext_ext = 0x00;  // extended extend byte
constexpr uint8_t sym_ack = 0x00;      // positive acknowledge
constexpr uint8_t sym_nak = 0xff;      // negative acknowledge
constexpr uint8_t sym_broad = 0xfe;    // broadcast destination address

// --- Protocol Limits ---
constexpr uint8_t max_bytes = 0x10;  // 16 maximum data bytes (Spec 5.6)
constexpr size_t default_sequence_capacity = 64;
constexpr uint8_t default_address = 0xff;
constexpr uint8_t default_lock_counter = 3;
constexpr uint8_t max_lock_counter = 25;

constexpr size_t num_handler_states = 15;
constexpr size_t num_request_states = 4;

// --- Default Values ---
constexpr size_t default_error_log_size = 10;
constexpr size_t default_history_size = 60;
constexpr size_t default_queue_size = 256;
constexpr size_t event_queue_capacity = 16;
constexpr size_t scheduler_item_capacity = 32;
constexpr uint32_t default_fsm_timeout_ms = 2000;
constexpr uint32_t default_client_timeout_ms = 1000;
constexpr uint32_t default_watchdog_timeout_ms = 5000;
constexpr uint32_t default_scheduler_total_timeout_ms = 4000;

// --- Transmission Defaults ---
constexpr int default_max_send_attempts = 3;
constexpr uint32_t default_base_backoff_ms = 100;

// --- Physical Timing Defaults ---
constexpr uint16_t default_window = 4300;
constexpr uint16_t min_window = 4000;
constexpr uint16_t max_window = 5000;
constexpr uint16_t default_offset = 80;
constexpr uint16_t max_offset = 500;

static_assert(default_offset < min_window,
              "The default offset must be smaller than the minimum arbitration "
              "window to prevent timing underflow.");

constexpr uint32_t default_syn_base_ms = 50;
constexpr uint32_t default_syn_tolerance_ms = 5;
constexpr uint32_t default_baud_rate = 2400;

// --- Posix Specific ---
constexpr const char* default_device_path = "/dev/ttyUSB0";

// --- Scanner Defaults ---
constexpr uint32_t default_initial_scan_delay_s = 10;
constexpr uint32_t default_startup_scan_interval_s = 60;
constexpr uint8_t default_max_startup_scans = 5;

// --- Physical Constants ---
constexpr double bit_time_us = 1000000.0 / 2400.0;  // ~416.67 us

// --- Resource Limits ---
constexpr size_t max_client_outbound_buffer = 4096;

}  // namespace ebus
