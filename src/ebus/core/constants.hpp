/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace ebus {
namespace internal {

// --- Resource Limits & Orchestration Defaults ---
constexpr size_t history_size = 60;
constexpr size_t queue_size = 256;
constexpr size_t event_queue_capacity = 16;
constexpr uint32_t controller_tick_ms = 20;

// --- HAL Constants ---
constexpr uint32_t arbitration_delay_us = 200;
constexpr uint32_t serialization_delay_ms = 4;
constexpr uint32_t carrier_sense_ms = 5;

// --- Component Defaults ---
struct Scheduler {
    static constexpr size_t item_capacity = 32;
    static constexpr size_t scan_threshold = 5;
};

struct Scanner {
    static constexpr uint32_t initial_delay_s = 10;
    static constexpr uint32_t startup_interval_s = 60;
    static constexpr uint8_t max_startup_scans = 5;
};

struct Network {
    static constexpr uint32_t transmit_timeout_ms = 500;
    static constexpr uint32_t wake_interval_ms = 20;
};

struct Syn {
    static constexpr uint32_t address_factor_ms = 10;
    static constexpr uint32_t postpone_ms = 2;
};

} // namespace internal
} // namespace ebus
