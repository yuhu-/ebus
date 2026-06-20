/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus::detail::enhanced {

/**
 * ebusd Enhanced Protocol (binary) constants and logic.
 */

enum class Command : uint8_t {
  init = 0x00,
  send = 0x01,
  start = 0x02,
  info = 0x03
};

enum class Response : uint8_t {
  resetted = 0x00,
  received = 0x01,
  started = 0x02,
  info = 0x03,
  failed = 0x0a,
  error_ebus = 0x0b,
  error_host = 0x0c
};

enum class Error : uint8_t { framing = 0x00, overrun = 0x01 };

struct Protocol {
  static inline void encode(Command cmd, uint8_t val, uint8_t out[2]) {
    encode(static_cast<uint8_t>(cmd), val, out);
  }

  static inline void encode(Response res, uint8_t val, uint8_t out[2]) {
    encode(static_cast<uint8_t>(res), val, out);
  }

  static inline void encode(uint8_t code, uint8_t val, uint8_t out[2]) {
    out[0] = 0xc0 | (code << 2) | (val >> 6);  // First byte: 11cc ccdd
    out[1] = 0x80 | (val & 0x3f);              // Second byte: 10dd dddd
  }

  template <typename T>
  static inline void decode(const uint8_t buf[2], T& cmd, uint8_t& val) {
    cmd = static_cast<T>((buf[0] >> 2) & 0x0f);      // Command in first byte
    val = ((buf[0] & 0x03) << 6) | (buf[1] & 0x3f);  // Value from both bytes
  }

  static inline bool isValidSequence(uint8_t b1, uint8_t b2) {
    return (b1 & 0xc0) == 0xc0 && (b2 & 0xc0) == 0x80;
  }
};

}  // namespace ebus::detail::enhanced