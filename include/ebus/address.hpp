/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus {

/**
 * Core eBUS byte symbols as defined in the specification.
 */
struct Symbols {
  static constexpr uint8_t zero = 0x00;     // zero byte
  static constexpr uint8_t syn = 0xaa;      // synchronization byte
  static constexpr uint8_t ext = 0xa9;      // extend byte
  static constexpr uint8_t syn_ext = 0x01;  // extended synchronization byte
  static constexpr uint8_t ext_ext = 0x00;  // extended extend byte
  static constexpr uint8_t ack = 0x00;      // positive acknowledge
  static constexpr uint8_t nak = 0xff;      // negative acknowledge
  static constexpr uint8_t broad = 0xfe;    // broadcast destination address

  /**
   * Returns true if the byte requires eBUS stuffing (0xAA or 0xA9).
   */
  static constexpr bool needsEscape(uint8_t byte) {
    return byte == syn || byte == ext;
  }

  /**
   * Escapes a byte according to eBUS rules (Spec 5.1).
   * out[0] is the escape byte (0xA9), out[1] is the escaped value.
   */
  static constexpr void escape(uint8_t byte, uint8_t out[2]) {
    out[0] = ext;
    out[1] = (byte == syn) ? syn_ext : ext_ext;
  }

  /**
   * Attempts to unescape an eBUS sequence.
   * Returns true if the sequence was a valid escape sequence.
   */
  static constexpr bool unescape(uint8_t b1, uint8_t b2, uint8_t& out) {
    if (b1 != ext) return false;
    if (b2 == syn_ext) {
      out = syn;
      return true;
    }
    if (b2 == ext_ext) {
      out = ext;
      return true;
    }
    return false;
  }
};

enum class TelegramType { undefined, broadcast, master_master, master_slave };

constexpr const char* toString(TelegramType type) noexcept {
  switch (type) {
    case TelegramType::broadcast:
      return "broadcast";
    case TelegramType::master_master:
      return "master_master";
    case TelegramType::master_slave:
      return "master_slave";
    default:
      return "unknown type";
  }
}

// --- Addressing logic ---

/**
 * Checks if a byte conforms to the eBUS master address rules (Spec 6.2.2.1).
 */
constexpr bool isMaster(uint8_t byte) {
  return ((((byte >> 4) & 0x0f) + 1) & ((byte >> 4) & 0x0f)) == 0 &&
         (((byte & 0x0f) + 1) & (byte & 0x0f)) == 0;
}

constexpr bool isSlave(uint8_t byte) {
  return !isMaster(byte) && byte != Symbols::syn && byte != Symbols::ext &&
         byte != Symbols::broad;
}

constexpr bool isTarget(uint8_t byte) {
  return byte != Symbols::syn && byte != Symbols::ext;
}

constexpr uint8_t masterOf(uint8_t byte) {
  if (byte == 0x04) return 0xff;
  if (byte < 5) return byte;
  uint8_t potential = static_cast<uint8_t>(byte - 5);
  return isMaster(potential) ? potential : byte;
}

constexpr uint8_t slaveOf(uint8_t byte) {
  if (byte == 0xff) return 0x04;
  if (!isMaster(byte)) return byte;
  uint8_t result = static_cast<uint8_t>(byte + 5);
  return isSlave(result) ? result : byte;
}

constexpr TelegramType typeOf(uint8_t byte) {
  if (byte == Symbols::broad) return TelegramType::broadcast;
  if (isMaster(byte)) return TelegramType::master_master;
  return TelegramType::master_slave;
}

}  // namespace ebus
