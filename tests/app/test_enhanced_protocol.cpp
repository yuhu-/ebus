/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/utils.hpp>

#include "app/enhanced_protocol.hpp"

using namespace ebus::detail::enhanced;

TEST_CASE("Encoding/decoding examples (0x7F / 0x80 / 0xFF)",
          "[app][enhanced]") {
  SECTION("Case 1: 0x7f (0111 1111)") {
    uint8_t out[2];
    uint8_t val;
    Command cmd;

    // cmd=1 (SEND), val=0x7F
    // byte 0: 11 <0001> <01> -> 1100 0101 -> 0xC5
    // byte 1: 10 <11 1111> -> 1011 1111 -> 0xBF
    Protocol::encode(Command::send, 0x7f, out);
    REQUIRE(out[0] == 0xc5);
    REQUIRE(out[1] == 0xbf);
    Protocol::decode(out, cmd, val);
    REQUIRE(cmd == Command::send);
    REQUIRE(val == 0x7f);
  }

  SECTION("Case 2: 0x80 (1000 0000)") {
    uint8_t out[2];
    uint8_t val;
    Command cmd;

    // cmd=1 (SEND), val=0x80
    // byte 0: 11 <0001> <10> -> 1100 0110 -> 0xC6
    Protocol::encode(Command::send, 0x80, out);
    REQUIRE(out[0] == 0xc6);
    REQUIRE(out[1] == 0x80);
    Protocol::decode(out, cmd, val);
    REQUIRE(cmd == Command::send);
    REQUIRE(val == 0x80);
  }

  SECTION("Case 3: 0xFF (1111 1111)") {
    uint8_t out[2];
    uint8_t val;
    Command cmd;

    // cmd=1 (SEND), val=0xFF
    // byte 0: 11 <0001> <11> -> 1100 0111 -> 0xC7
    // byte 1: 10 <11 1111> -> 1011 1111 -> 0xBF
    Protocol::encode(Command::send, 0xff, out);
    REQUIRE(out[0] == 0xc7);
    REQUIRE(out[1] == 0xbf);
    Protocol::decode(out, cmd, val);
    REQUIRE(cmd == Command::send);
    REQUIRE(val == 0xff);
  }
}

TEST_CASE("Validation of sequence prefixes", "[app][enhanced]") {
  SECTION("Valid sequence: 11xxxxxx 10xxxxxx") {
    REQUIRE(Protocol::isValidSequence(0xc0, 0x80));
    REQUIRE(Protocol::isValidSequence(0xff, 0xbf));
  }

  SECTION("Invalid first byte prefix (must be 11)") {
    REQUIRE(!Protocol::isValidSequence(0x00, 0x80));
    REQUIRE(!Protocol::isValidSequence(0x40, 0x80));
    REQUIRE(!Protocol::isValidSequence(0x80, 0x80));
  }

  SECTION("Invalid second byte prefix (must be 10)") {
    REQUIRE(!Protocol::isValidSequence(0xc0, 0x00));
    REQUIRE(!Protocol::isValidSequence(0xc0, 0x40));
    REQUIRE(!Protocol::isValidSequence(0xc0, 0xc0));
  }
}

TEST_CASE("Decode extracts bits regardless of prefix", "[app][enhanced]") {
  SECTION("Decode robustness (prefix independence)") {
    uint8_t val = 0;
    Command cmd;
    // CMD_SEND (1), Val 0xaa.
    // Logic: Correct wire format is 0xc6 0xaa.
    // We test with malformed prefixes (00 instead of 11/10) to ensure
    // masking works.
    uint8_t malformed[] = {0x06, 0x2a};
    Protocol::decode(malformed, cmd, val);
    REQUIRE(cmd == Command::send);
    REQUIRE(val == 0xaa);
  }
}

TEST_CASE("Exhaustive roundtrip (4096 cases)", "[app][enhanced]") {
  SECTION("Exhaustive roundtrip (commands 0..15, values 0..255)") {
    for (int c = 0; c < 16; ++c) {
      for (int v = 0; v < 256; ++v) {
        uint8_t encoded[2], dVal;
        Command dCmd;
        Protocol::encode(c, v, encoded);
        Protocol::decode(encoded, dCmd, dVal);
        REQUIRE(dCmd == static_cast<Command>(c));
        REQUIRE(dVal == v);
      }
    }
  }
}
