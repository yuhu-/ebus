/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <vector>

TEST_CASE("Sequence Extend and Reduce logic", "[core][sequence]") {
  ebus::Sequence seq;

  SECTION("Empty sequences remain empty") {
    seq.assign(ebus::ByteView{}, false);
    seq.extend();
    REQUIRE(seq.toVector().empty());
  }

  SECTION("Normal sequences are not modified") {
    auto data = ebus::toVector("010203");
    seq.assign(data, false);
    seq.extend();
    REQUIRE(seq.toVector() == data);
  }

  SECTION("syn (0xaa) is escaped to a9 01") {
    seq.assign(ebus::toVector("01aa03"), false);
    seq.extend();
    REQUIRE(seq.toVector() == ebus::toVector("01a90103"));

    seq.reduce();
    REQUIRE(seq.toVector() == ebus::toVector("01aa03"));
  }

  SECTION("ESC (0xa9) is escaped to a9 00") {
    seq.assign(ebus::toVector("01a903"), false);
    seq.extend();
    REQUIRE(seq.toVector() == ebus::toVector("01a90003"));
  }

  SECTION("Truncated escape (lone a9) at end of sequence") {
    seq.assign(ebus::toVector("01a9"), true);
    seq.reduce();
    REQUIRE(seq.toVector() == ebus::toVector("01a9"));
    REQUIRE_FALSE(seq.isExtended());
  }
}

TEST_CASE("Sequence CRC Calculation", "[core][sequence]") {
  ebus::Sequence seq;

  SECTION("Standard telegram CRC") {
    // crc("1008b511020300") -> 0x1e
    seq.assign(ebus::toVector("1008b511020300"), false);
    REQUIRE(seq.crc() == 0x1e);

    seq.extend();
    REQUIRE(seq.crc() == 0x1e);  // CRC must ignore escapes
  }

  SECTION("CRC with byte stuffing required") {
    seq.assign(ebus::toVector("01aa03"), false);
    REQUIRE(seq.crc() == 0x22);
  }
}

TEST_CASE("Sequence Operators", "[core][sequence]") {
  ebus::Sequence s1, s2;
  s1.assign(ebus::toVector("010203"), false);

  SECTION("Equality comparison") {
    s2.assign(ebus::toVector("010203"), false);
    REQUIRE(s1 == s2);
    REQUIRE_FALSE(s1 != s2);
  }

  SECTION("Different content") {
    s2.assign(ebus::toVector("010204"), false);
    REQUIRE(s1 != s2);
  }
}

TEST_CASE("Sequence Append operations", "[core][sequence]") {
  ebus::Sequence s1, s2;

  SECTION("Append Extended to Reduced triggers auto-reduction") {
    s1.assign(ebus::toVector("0102"), false);
    s2.assign(ebus::toVector("a901"), true);  // logical AA
    s1.append(s2);
    REQUIRE(s1.toVector() == ebus::toVector("0102aa"));
  }

  SECTION("Append Reduced to Extended triggers auto-extension") {
    s1.assign(ebus::toVector("a901"), true);
    s2.assign(ebus::toVector("aa"), false);
    s1.append(s2);
    REQUIRE(s1.toVector() == ebus::toVector("a901a901"));
  }
}

TEST_CASE("Sequence Logical Comparison", "[core][sequence]") {
  ebus::Sequence reduced;
  ebus::Sequence extended;

  reduced.assign(ebus::toVector("01aa03"), false);
  extended.assign(ebus::toVector("01a90103"), true);

  REQUIRE(reduced != extended);                // Physical bytes differ
  REQUIRE(reduced.logicallyEquals(extended));  // Protocol meaning is the same
}

TEST_CASE("Sequence: SBO Overflow", "[core][sequence]") {
  ebus::Sequence seq;
  std::vector<uint8_t> large_data(100, 0x11);
  large_data[50] = 0xAA;  // Forces expansion

  seq.assign(large_data, false);
  seq.extend();

  REQUIRE(seq.size() == 101);
  REQUIRE(seq.toVector()[50] == 0xA9);
  REQUIRE(seq.toVector()[51] == 0x01);
}
