/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "Core/Sequence.hpp"
#include "Utils/Common.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_extend_reduce() {
  // Verifies the byte-stuffing logic:
  // Reduced (Raw) <-> Extended (Wire format with escapes)
  std::cout << "\n=== Test: Sequence Extend/Reduce Logic ===" << std::endl;

  struct TestCase {
    std::string description;
    std::string reduced_hex;
    std::string extended_hex;
  };

  std::vector<TestCase> test_cases = {
      {"Empty sequence", "", ""},
      {"Normal sequence", "010203", "010203"},
      {"Sequence with SYN", "01aa03", "01a90103"},
      {"Sequence with ESC", "01a903", "01a90003"},
      {"Sequence with both", "aa01a9", "a90101a900"},
      {"Sequence starting with SYN", "aa0102", "a9010102"},
      {"Sequence ending with SYN", "0102aa", "0102a901"},
      {"Sequence starting with ESC", "a90102", "a9000102"},
      {"Sequence ending with ESC", "0102a9", "0102a900"},
      {"Double SYN", "aaaa", "a901a901"},
      {"Double ESC", "a9a9", "a900a900"},
      {"Alternating SYN/ESC", "aaa9aa", "a901a900a901"},
  };

  for (const auto& tc : test_cases) {
    ebus::Sequence seq;
    std::vector<uint8_t> reduced_vec = ebus::to_vector(tc.reduced_hex);
    std::vector<uint8_t> extended_vec = ebus::to_vector(tc.extended_hex);

    // Test extend
    seq.assign(reduced_vec, false);
    seq.extend();
    run_test(tc.description + " (extend)", seq.to_vector() == extended_vec);

    // Test reduce
    seq.assign(extended_vec, true);
    seq.reduce();
    run_test(tc.description + " (reduce)", seq.to_vector() == reduced_vec);
  }
}

void test_crc() {
  // Verifies CRC calculation over extended sequences (as required by spec).
  std::cout << "\n=== Test: Sequence CRC Calculation ===" << std::endl;
  ebus::Sequence seq;

  // QQ ZZ NN PB SB D1 D2 CRC
  // 10 08 b5 11 02 03 00 1e
  // crc("1008b511020300") -> 0x1e
  seq.assign(ebus::to_vector("1008b511020300"), true);
  run_test("CRC calculation on reduced seq", seq.crc() == 0x1e);

  // CRC should be the same whether the sequence is currently extended or not
  seq.extend();
  run_test("CRC calculation on extended seq", seq.crc() == 0x1e);

  // Test CRC on a sequence that requires byte-stuffing
  // reduced: 01aa03 -> extended: 01a90103
  // The CRC must be calculated on the extended sequence.
  seq.assign(ebus::to_vector("01aa03"), false);
  run_test("CRC calculation with byte stuffing", seq.crc() == 0x22);
}

void test_operators() {
  std::cout << "\n=== Test: Sequence Operators ===" << std::endl;
  ebus::Sequence s1, s2;

  s1.assign(ebus::to_vector("010203"), false);
  s2.assign(ebus::to_vector("010203"), false);
  run_test("Equality check identical", s1 == s2);
  run_test("Inequality check identical", !(s1 != s2));

  s2.assign(ebus::to_vector("010204"), false);
  run_test("Equality check different content", !(s1 == s2));
  run_test("Inequality check different content", s1 != s2);

  // Same data but different extended state
  s2.assign(ebus::to_vector("010203"), true);
  run_test("Equality check different extended state", !(s1 == s2));
}

void test_append() {
  // Verifies that appending sequences correctly handles mixed states
  // (e.g., appending a reduced sequence to an extended one).
  std::cout << "\n=== Test: Sequence Append ===" << std::endl;

  // Case 1: Reduced + Reduced
  ebus::Sequence s1, s2, expected;
  s1.assign(ebus::to_vector("0102"), false);
  s2.assign(ebus::to_vector("0304"), false);
  s1.append(s2);
  expected.assign(ebus::to_vector("01020304"), false);
  run_test("Append Reduced+Reduced", s1 == expected);

  // Case 2: Extended + Extended
  // 0xAA -> 0xA9 0x01
  s1.assign(ebus::to_vector("a901"), true);
  s2.assign(ebus::to_vector("a901"), true);
  s1.append(s2);
  // Expected: a901 a901
  expected.assign(ebus::to_vector("a901a901"), true);
  run_test("Append Extended+Extended", s1 == expected);

  // Case 3: Reduced + Extended (s1 is reduced, s2 is extended)
  // s1 = 01 02 (reduced)
  // s2 = a9 01 (extended, which is AA)
  // result should be reduced: 01 02 aa
  s1.assign(ebus::to_vector("0102"), false);
  s2.assign(ebus::to_vector("a901"), true);
  s1.append(s2);
  expected.assign(ebus::to_vector("0102aa"), false);
  run_test("Append Reduced+Extended (auto-reduce)", s1 == expected);

  // Case 4: Extended + Reduced (s1 is extended, s2 is reduced)
  // s1 = a9 01 (extended AA)
  // s2 = aa (reduced)
  // result should be extended: a9 01 a9 01
  s1.assign(ebus::to_vector("a901"), true);
  s2.assign(ebus::to_vector("aa"), false);
  s1.append(s2);
  expected.assign(ebus::to_vector("a901a901"), true);
  run_test("Append Extended+Reduced (auto-extend)", s1 == expected);
}

int main() {
  test_extend_reduce();
  test_crc();
  test_operators();
  test_append();

  std::cout << "\nAll sequence tests passed!" << std::endl;

  return EXIT_SUCCESS;
}