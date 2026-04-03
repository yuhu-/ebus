/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <vector>
#include <cassert>
#include "App/EnhancedProtocol.hpp"
#include "Utils/Common.hpp"

/**
 * Unit test for the ebusd Enhanced Protocol bit-shifting logic.
 */

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED") << std::endl;
  if (!condition) std::exit(1);
}

void test_protocol_logic() {
  std::cout << "--- Test: Enhanced Protocol Logic (0x7f/0x80) ---" << std::endl;

  uint8_t out[2];
  uint8_t cmd, val;

  // Case 1: 0x7f (0111 1111) 
  // cmd=1 (SEND), val=0x7f
  // byte 0: 11 <0001> <01> -> 1100 0101 -> 0xc5
  // byte 1: 10 <11 1111> -> 1011 1111 -> 0xbf
  ebus::enhanced::Protocol::encode(ebus::enhanced::CMD_SEND, 0x7f, out);
  run_test("0x7f encoded high byte (0xc5)", out[0] == 0xc5);
  run_test("0x7f encoded low byte (0xbf)", out[1] == 0xbf);
  
  ebus::enhanced::Protocol::decode(out, cmd, val);
  run_test("0x7f decoded correctly", cmd == ebus::enhanced::CMD_SEND && val == 0x7f);

  // Case 2: 0x80 (1000 0000)
  // byte 0: 11 <0001> <10> -> 1100 0110 -> 0xc6
  // byte 1: 10 <00 0000> -> 1000 0000 -> 0x80
  ebus::enhanced::Protocol::encode(ebus::enhanced::CMD_SEND, 0x80, out);
  run_test("0x80 encoded correctly", out[0] == 0xc6 && out[1] == 0x80);

  ebus::enhanced::Protocol::decode(out, cmd, val);
  run_test("0x80 decoded correctly", cmd == ebus::enhanced::CMD_SEND && val == 0x80);

  // Case 3: 0xff (1111 1111)
  // byte 0: 11 <0001> <11> -> 1100 0111 -> 0xc7
  // byte 1: 10 <11 1111> -> 1011 1111 -> 0xbf
  ebus::enhanced::Protocol::encode(ebus::enhanced::CMD_SEND, 0xff, out);
  run_test("0xff encoded correctly", out[0] == 0xc7 && out[1] == 0xbf);
}

void test_validation() {
  std::cout << "--- Test: Enhanced Protocol Validation ---" << std::endl;

  // Valid sequence: 11xxxxxx 10xxxxxx
  run_test("Valid sequence (0xc0, 0x80)",
           ebus::enhanced::Protocol::isValidSequence(0xc0, 0x80));
  run_test("Valid sequence (0xff, 0xbf)",
           ebus::enhanced::Protocol::isValidSequence(0xff, 0xbf));

  // Invalid first byte prefix (must be 11)
  run_test("Invalid B1 prefix (0x00)",
           !ebus::enhanced::Protocol::isValidSequence(0x00, 0x80));
  run_test("Invalid B1 prefix (0x40)",
           !ebus::enhanced::Protocol::isValidSequence(0x40, 0x80));
  run_test("Invalid B1 prefix (0x80)",
           !ebus::enhanced::Protocol::isValidSequence(0x80, 0x80));

  // Invalid second byte prefix (must be 10)
  run_test("Invalid B2 prefix (0x00)",
           !ebus::enhanced::Protocol::isValidSequence(0xc0, 0x00));
  run_test("Invalid B2 prefix (0x40)",
           !ebus::enhanced::Protocol::isValidSequence(0xc0, 0x40));
  run_test("Invalid B2 prefix (0xc0)",
           !ebus::enhanced::Protocol::isValidSequence(0xc0, 0xc0));
}

void test_decode_robustness() {
  std::cout << "--- Test: Decode Robustness (Prefix Independence) ---" << std::endl;
  uint8_t cmd, val;
  // CMD_SEND (1), Val 0xaa.
  // Logic: Correct wire format is 0xc6 0xaa.
  // We test with malformed prefixes (00 instead of 11/10) to ensure masking works.
  uint8_t malformed[] = {0x06, 0x2a}; 
  ebus::enhanced::Protocol::decode(malformed, cmd, val);
  run_test("Decode extracts bits regardless of prefix", 
           cmd == ebus::enhanced::CMD_SEND && val == 0xaa);
}

int main() {
  test_protocol_logic();
  test_validation();
  test_decode_robustness();

  // Exhaustive test for all possible 8-bit values and commands
  std::cout << "--- Test: Protocol Exhaustive 8-bit Roundtrip ---" << std::endl;
  bool success = true;
  for (int c = 0; c < 16; ++c) {
    for (int v = 0; v < 256; ++v) {
      uint8_t encoded[2], dCmd, dVal;
      ebus::enhanced::Protocol::encode(c, v, encoded);
      ebus::enhanced::Protocol::decode(encoded, dCmd, dVal);
      if (dCmd != c || dVal != v) success = false;
    }
  }
  run_test("Exhaustive roundtrip (4096 cases)", success);

  std::cout << "\nAll EnhancedProtocol unit tests passed!" << std::endl;
  return 0;
}