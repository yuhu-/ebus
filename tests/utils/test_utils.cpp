/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <ebus/callbacks.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/device.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <vector>

using namespace ebus::detail;

TEST_CASE("Utils: Addressing logic", "[utils][utils]") {
  REQUIRE(ebus::isMaster(0x00));
  REQUIRE(ebus::isMaster(0x01));
  REQUIRE(ebus::isMaster(0x03));
  REQUIRE(ebus::isMaster(0x10));
  REQUIRE(ebus::isMaster(0x37));
  REQUIRE(ebus::isMaster(0xff));

  REQUIRE(!ebus::isMaster(0x02));
  REQUIRE(!ebus::isMaster(0x04));
  REQUIRE(!ebus::isMaster(0x05));
  REQUIRE(!ebus::isMaster(ebus::Symbols::syn));

  REQUIRE(ebus::isSlave(0x05));
  REQUIRE(!ebus::isSlave(0x00));
  REQUIRE(!ebus::isSlave(ebus::Symbols::syn));

  REQUIRE(ebus::masterOf(0x05) == 0x00);
  REQUIRE(ebus::masterOf(0x00) == 0x00);

  REQUIRE(ebus::slaveOf(0x00) == 0x05);
  REQUIRE(ebus::slaveOf(0x05) == 0x05);

  REQUIRE(ebus::masterOf(0x00) == 0x00);
  REQUIRE(ebus::slaveOf(0xff) == 0x04);
}

TEST_CASE("Utils: Conversions", "[utils][utils]") {
  REQUIRE(std::string_view(ebus::toString(0x0a)) == "0a");
  REQUIRE(std::string_view(ebus::toString(0xff)) == "ff");

  std::vector<uint8_t> vec = {0x01, 0x02, 0xff};
  REQUIRE(ebus::toString(vec) == "0102ff");

  std::vector<uint8_t> res = ebus::toVector("0102ff");
  REQUIRE(res.size() == 3);
  REQUIRE(res == vec);
}

TEST_CASE("Utils: Vector utilities", "[utils][utils]") {
  std::vector<uint8_t> vec = {0x10, 0x20, 0x30, 0x40, 0x50};

  ebus::ByteView sub = ebus::range(vec, 1, 3);  // 20 30 40
  ebus::ByteView expected_sub(vec.data() + 1, 3);
  REQUIRE(sub == expected_sub);
  REQUIRE(ebus::range(vec, 10, 1).empty());

  std::vector<uint8_t> search_hit = {0x30, 0x40};
  std::vector<uint8_t> search_miss = {0x30, 0x50};

  REQUIRE(ebus::contains(vec, search_hit));
  REQUIRE(!ebus::contains(vec, search_miss));

  REQUIRE(ebus::matches(vec, search_hit, 2));
  REQUIRE(!ebus::matches(vec, search_hit, 0));
}

TEST_CASE("Utils: CRC", "[utils][utils]") {
  // calc_crc(byte, 0) == byte because table[0] == 0
  REQUIRE(ebus::calcCRC(0x77, 0x00) == 0x77);

  // calc_crc(0, init) == table[init] (table[1] == 0x9b)
  REQUIRE(ebus::calcCRC(0x00, 0x01) == 0x9b);

  // Manual chain verification: 10 08 -> 3a
  uint8_t crc = 0;
  crc = ebus::calcCRC(0x10, crc);
  crc = ebus::calcCRC(0x08, crc);
  REQUIRE(crc == 0x3a);

  // Full sequence verification
  std::vector<uint8_t> data = ebus::toVector("1008b511020300");
  crc = 0;
  for (uint8_t b : data) crc = ebus::calcCRC(b, crc);
  REQUIRE(crc == 0x1e);
}
