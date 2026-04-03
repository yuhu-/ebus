/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Utils/Common.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_addressing() {
  std::cout << "\n=== Test: Addressing logic ===" << std::endl;

  // Test isMaster
  // Valid master nibbles: 0, 1, 3, 7, F
  run_test("isMaster(0x00)", ebus::isMaster(0x00));
  run_test("isMaster(0x01)", ebus::isMaster(0x01));
  run_test("isMaster(0x03)", ebus::isMaster(0x03));
  run_test("isMaster(0x10)", ebus::isMaster(0x10));
  run_test("isMaster(0x37)", ebus::isMaster(0x37));
  run_test("isMaster(0xFF)", ebus::isMaster(0xFF));

  run_test("!isMaster(0x02)", !ebus::isMaster(0x02));
  run_test("!isMaster(0x04)", !ebus::isMaster(0x04));
  run_test("!isMaster(0x05)", !ebus::isMaster(0x05));
  run_test("!isMaster(0xAA) [SYN]", !ebus::isMaster(ebus::sym_syn));

  // Test isSlave
  // Slaves are usually Master + 5.
  run_test("isSlave(0x05)", ebus::isSlave(0x05));
  run_test("isSlave(0x00) [Master is not Slave]", !ebus::isSlave(0x00));
  run_test("isSlave(0xAA) [SYN is not Slave]", !ebus::isSlave(ebus::sym_syn));

  // Test masterOf / slaveOf
  run_test("masterOf(0x05) == 0x00", ebus::masterOf(0x05) == 0x00);
  run_test("masterOf(0x00) == 0x00",
           ebus::masterOf(0x00) == 0x00);  // Already master

  run_test("slaveOf(0x00) == 0x05", ebus::slaveOf(0x00) == 0x05);
  run_test("slaveOf(0x05) == 0x05",
           ebus::slaveOf(0x05) == 0x05);  // Already slave
}

void test_conversions() {
  std::cout << "\n=== Test: Conversions ===" << std::endl;

  // to_string
  run_test("to_string(0x0A)", ebus::to_string(0x0A) == "0a");
  run_test("to_string(0xFF)", ebus::to_string(0xFF) == "ff");

  std::vector<uint8_t> vec = {0x01, 0x02, 0xFF};
  run_test("to_string(vector)", ebus::to_string(vec) == "0102ff");

  // to_vector
  std::vector<uint8_t> res = ebus::to_vector("0102ff");
  run_test("to_vector size", res.size() == 3);
  run_test("to_vector content", res == vec);
}

void test_vector_utils() {
  std::cout << "\n=== Test: Vector Utils ===" << std::endl;

  std::vector<uint8_t> vec = {0x10, 0x20, 0x30, 0x40, 0x50};

  // Range
  std::vector<uint8_t> sub = ebus::range(vec, 1, 3);  // 20 30 40
  std::vector<uint8_t> expected_sub = {0x20, 0x30, 0x40};
  run_test("range(1,3)", sub == expected_sub);
  run_test("range out of bounds", ebus::range(vec, 10, 1).empty());

  // Contains / Matches
  std::vector<uint8_t> search_hit = {0x30, 0x40};
  std::vector<uint8_t> search_miss = {0x30, 0x50};

  run_test("contains hit", ebus::contains(vec, search_hit));
  run_test("contains miss", !ebus::contains(vec, search_miss));

  run_test("matches index 2", ebus::matches(vec, search_hit, 2));
  run_test("matches index 0 (false)", !ebus::matches(vec, search_hit, 0));
}

void test_crc() {
  std::cout << "\n=== Test: CRC ===" << std::endl;

  // 1. Basic property: calc_crc(byte, 0) == byte because table[0] == 0
  run_test("CRC step (init=0)", ebus::calc_crc(0x77, 0x00) == 0x77);

  // 2. Basic property: calc_crc(0, init) == table[init] (table[1] = 0x9b)
  run_test("CRC step (init=1, byte=0)", ebus::calc_crc(0x00, 0x01) == 0x9b);

  // 3. Manual chain verification: 10 08 -> 3a
  uint8_t crc = 0;
  crc = ebus::calc_crc(0x10, crc);
  crc = ebus::calc_crc(0x08, crc);
  run_test("CRC chain (10 08 -> 3a)", crc == 0x3a);

  // 4. Full sequence verification matching a known valid telegram
  // Data: 10 08 b5 11 02 03 00 -> CRC 0x1e
  std::vector<uint8_t> data = ebus::to_vector("1008b511020300");
  crc = 0;
  for (uint8_t b : data) {
    crc = ebus::calc_crc(b, crc);
  }
  run_test("CRC full sequence (1008b511020300 -> 1e)", crc == 0x1e);
}

int main() {
  test_addressing();
  test_conversions();
  test_vector_utils();
  test_crc();

  std::cout << "\nAll common tests passed!" << std::endl;
  return EXIT_SUCCESS;
}
