/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

#include "ebus/constants.hpp"
#include "ebus/enums.hpp"

namespace ebus {

/**
 * Checks if a byte conforms to the eBUS master address rules (Spec 6.2.2.1).
 * Valid values x satisfy ((x + 1) & x) == 0 for both nibbles (value <= 15).
 */
constexpr bool isMaster(uint8_t byte) {
  return ((((byte >> 4) & 0x0f) + 1) & ((byte >> 4) & 0x0f)) == 0 &&
         (((byte & 0x0f) + 1) & (byte & 0x0f)) == 0;
}

/**
 * Checks if a byte is a slave address (not master, not SYN, EXT, or BROADCAST).
 */
constexpr bool isSlave(uint8_t byte) {
  return !isMaster(byte) && byte != sym_syn && byte != sym_ext &&
         byte != sym_broad;
}

/**
 * Checks if a byte is a valid target address (not SYN or EXT).
 */
constexpr bool isTarget(uint8_t byte) {
  return byte != sym_syn && byte != sym_ext;
}

/**
 * Returns a valid master address or the given byte.
 */
constexpr uint8_t masterOf(uint8_t byte) {
  if (byte == 0x04)
    return 0xff;  // Special Case: Slave 04h belongs to Master FFh
  if (byte < 5) return byte;
  uint8_t potential = static_cast<uint8_t>(byte - 5);
  return isMaster(potential) ? potential : byte;
}

/**
 * Returns a valid slave address or the given byte.
 */
constexpr uint8_t slaveOf(uint8_t byte) {
  if (byte == 0xff) return 0x04;  // Special Case: Master FFh has Slave 04h
  if (!isMaster(byte)) return byte;
  // Slaves are defined as Master Address + 5
  uint8_t result = static_cast<uint8_t>(byte + 5);
  return isSlave(result) ? result : byte;
}

/**
 * Determines the telegram type based on the destination address.
 */
constexpr TelegramType typeOf(uint8_t byte) {
  if (byte == sym_broad) return TelegramType::broadcast;
  if (isMaster(byte)) return TelegramType::master_master;
  return TelegramType::master_slave;
}

}  // namespace ebus
