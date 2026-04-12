/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

#include "core/sequence.hpp"
#include "utils/common.hpp"

TEST_CASE("Sequence Extend and Reduce logic", "[core][sequence]") {
  ebus::Sequence seq;

  SECTION("Empty sequences remain empty") {
    seq.assign({}, false);
    seq.extend();
    REQUIRE(seq.to_vector().empty());
  }

  SECTION("Normal sequences are not modified") {
    auto data = ebus::to_vector("010203");
    seq.assign(data, false);
    seq.extend();
    REQUIRE(seq.to_vector() == data);
  }

  SECTION("SYN (0xAA) is escaped to A9 01") {
    seq.assign(ebus::to_vector("01aa03"), false);
    seq.extend();
    REQUIRE(seq.to_vector() == ebus::to_vector("01a90103"));

    seq.reduce();
    REQUIRE(seq.to_vector() == ebus::to_vector("01aa03"));
  }

  SECTION("ESC (0xA9) is escaped to A9 00") {
    seq.assign(ebus::to_vector("01a903"), false);
    seq.extend();
    REQUIRE(seq.to_vector() == ebus::to_vector("01a90003"));
  }
}

TEST_CASE("Sequence CRC Calculation", "[core][sequence]") {
  ebus::Sequence seq;

  SECTION("Standard telegram CRC") {
    // crc("1008b511020300") -> 0x1e
    seq.assign(ebus::to_vector("1008b511020300"), false);
    REQUIRE(seq.crc() == 0x1e);

    seq.extend();
    REQUIRE(seq.crc() == 0x1e);  // CRC must ignore escapes
  }

  SECTION("CRC with byte stuffing required") {
    seq.assign(ebus::to_vector("01aa03"), false);
    REQUIRE(seq.crc() == 0x22);
  }
}

TEST_CASE("Sequence Operators", "[core][sequence]") {
  ebus::Sequence s1, s2;
  s1.assign(ebus::to_vector("010203"), false);

  SECTION("Equality comparison") {
    s2.assign(ebus::to_vector("010203"), false);
    REQUIRE(s1 == s2);
    REQUIRE_FALSE(s1 != s2);
  }

  SECTION("Different content") {
    s2.assign(ebus::to_vector("010204"), false);
    REQUIRE(s1 != s2);
  }
}

TEST_CASE("Sequence Append operations", "[core][sequence]") {
  ebus::Sequence s1, s2;

  SECTION("Append Extended to Reduced triggers auto-reduction") {
    s1.assign(ebus::to_vector("0102"), false);
    s2.assign(ebus::to_vector("a901"), true);  // logical AA
    s1.append(s2);
    REQUIRE(s1.to_vector() == ebus::to_vector("0102aa"));
  }

  SECTION("Append Reduced to Extended triggers auto-extension") {
    s1.assign(ebus::to_vector("a901"), true);
    s2.assign(ebus::to_vector("aa"), false);
    s1.append(s2);
    REQUIRE(s1.to_vector() == ebus::to_vector("a901a901"));
  }
}
