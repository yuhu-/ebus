/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus {

/**
 * ebusd Enhanced Protocol (binary) constants and logic.
 */
namespace enhanced {

enum Command : uint8_t {
  CMD_INIT = 0x00,
  CMD_SEND = 0x01,
  CMD_START = 0x02,
  CMD_INFO = 0x03
};

enum Response : uint8_t {
  RESP_RESETTED = 0x00,
  RESP_RECEIVED = 0x01,
  RESP_STARTED = 0x02,
  RESP_INFO = 0x03,
  RESP_FAILED = 0x0a,
  RESP_ERROR_EBUS = 0x0b,
  RESP_ERROR_HOST = 0x0c
};

enum Error : uint8_t { ERR_FRAMING = 0x00, ERR_OVERRUN = 0x01 };

struct Protocol {
  static inline void encode(uint8_t cmd, uint8_t val, uint8_t out[2]) {
    out[0] = 0xc0 | (cmd << 2) | (val >> 6);
    out[1] = 0x80 | (val & 0x3f);
  }

  static inline void decode(const uint8_t buf[2], uint8_t& cmd, uint8_t& val) {
    cmd = (buf[0] >> 2) & 0x0f;
    val = ((buf[0] & 0x03) << 6) | (buf[1] & 0x3f);
  }

  static inline bool isValidSequence(uint8_t b1, uint8_t b2) {
    return (b1 & 0xc0) == 0xc0 && (b2 & 0xc0) == 0x80;
  }
};

}  // namespace enhanced
}  // namespace ebus