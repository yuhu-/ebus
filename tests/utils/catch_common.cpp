/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

#include "utils/common.hpp"

TEST_CASE("Common: Addressing logic", "[utils][common]") {
  REQUIRE(ebus::isMaster(0x00));
  REQUIRE(ebus::isMaster(0x01));
  REQUIRE(ebus::isMaster(0x03));
  REQUIRE(ebus::isMaster(0x10));
  REQUIRE(ebus::isMaster(0x37));
  REQUIRE(ebus::isMaster(0xFF));

  REQUIRE(!ebus::isMaster(0x02));
  REQUIRE(!ebus::isMaster(0x04));
  REQUIRE(!ebus::isMaster(0x05));
  REQUIRE(!ebus::isMaster(ebus::sym_syn));

  REQUIRE(ebus::isSlave(0x05));
  REQUIRE(!ebus::isSlave(0x00));
  REQUIRE(!ebus::isSlave(ebus::sym_syn));

  REQUIRE(ebus::masterOf(0x05) == 0x00);
  REQUIRE(ebus::masterOf(0x00) == 0x00);

  REQUIRE(ebus::slaveOf(0x00) == 0x05);
  REQUIRE(ebus::slaveOf(0x05) == 0x05);
}

TEST_CASE("Common: Conversions", "[utils][common]") {
  REQUIRE(ebus::toString(0x0a) == "0a");
  REQUIRE(ebus::toString(0xff) == "ff");

  std::vector<uint8_t> vec = {0x01, 0x02, 0xff};
  REQUIRE(ebus::toString(vec) == "0102ff");

  std::vector<uint8_t> res = ebus::toVector("0102ff");
  REQUIRE(res.size() == 3);
  REQUIRE(res == vec);
}

TEST_CASE("Common: Vector utilities", "[utils][common]") {
  std::vector<uint8_t> vec = {0x10, 0x20, 0x30, 0x40, 0x50};

  std::vector<uint8_t> sub = ebus::range(vec, 1, 3);  // 20 30 40
  std::vector<uint8_t> expected_sub = {0x20, 0x30, 0x40};
  REQUIRE(sub == expected_sub);
  REQUIRE(ebus::range(vec, 10, 1).empty());

  std::vector<uint8_t> search_hit = {0x30, 0x40};
  std::vector<uint8_t> search_miss = {0x30, 0x50};

  REQUIRE(ebus::contains(vec, search_hit));
  REQUIRE(!ebus::contains(vec, search_miss));

  REQUIRE(ebus::matches(vec, search_hit, 2));
  REQUIRE(!ebus::matches(vec, search_hit, 0));
}

TEST_CASE("Common: CRC", "[utils][common]") {
  // calc_crc(byte, 0) == byte because table[0] == 0
  REQUIRE(ebus::calcCrc(0x77, 0x00) == 0x77);

  // calc_crc(0, init) == table[init] (table[1] == 0x9b)
  REQUIRE(ebus::calcCrc(0x00, 0x01) == 0x9b);

  // Manual chain verification: 10 08 -> 3a
  uint8_t crc = 0;
  crc = ebus::calcCrc(0x10, crc);
  crc = ebus::calcCrc(0x08, crc);
  REQUIRE(crc == 0x3a);

  // Full sequence verification
  std::vector<uint8_t> data = ebus::toVector("1008b511020300");
  crc = 0;
  for (uint8_t b : data) crc = ebus::calcCrc(b, crc);
  REQUIRE(crc == 0x1e);
}