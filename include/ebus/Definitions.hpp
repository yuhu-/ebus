/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ebus {

// --- Protocol Symbols ---
constexpr uint8_t sym_zero = 0x00;      // zero byte
constexpr uint8_t sym_syn = 0xaa;      // synchronization byte
constexpr uint8_t sym_ext = 0xa9;      // extend byte
constexpr uint8_t sym_syn_ext = 0x01;  // extended synchronization byte
constexpr uint8_t sym_ext_ext = 0x00;  // extended extend byte
constexpr uint8_t sym_ack = 0x00;      // positive acknowledge
constexpr uint8_t sym_nak = 0xff;      // negative acknowledge
constexpr uint8_t sym_broad = 0xfe;    // broadcast destination address

// --- Protocol Limits ---
constexpr uint8_t max_bytes = 0x10;  // 16 maximum data bytes (Spec 5.6)

// --- Protocol Enums ---
enum class TelegramType { undefined, broadcast, master_master, master_slave };
enum class MessageType { undefined, active, passive, reactive };

// --- Public Callback Signatures ---

/**
 * Callback for every valid telegram captured on the bus.
 */
using TelegramCallback = std::function<void(
    const MessageType& messageType, const TelegramType& telegramType,
    const std::vector<uint8_t>& master, const std::vector<uint8_t>& slave)>;

/**
 * Callback for protocol or hardware errors.
 */
using ErrorCallback = std::function<void(const std::string& errorMessage,
                                         const std::vector<uint8_t>& master,
                                         const std::vector<uint8_t>& slave)>;

/**
 * Result of a message enqueued via the Scheduler.
 */
using ResultCallback = std::function<void(
    bool success, const std::vector<uint8_t>& master,
    const std::vector<uint8_t>& slave)>;

}  // namespace ebus