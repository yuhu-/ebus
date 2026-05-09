/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <vector>
#include "utils/circular_buffer.hpp"

using namespace ebus::detail;

TEST_CASE("CircularBuffer: Behavior", "[utils][circularbuffer]") {
  CircularBuffer<int, 3> buffer;

  SECTION("Basic push and access") {
    buffer.push_back(1);
    buffer.push_back(2);
    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer[0] == 1);
    REQUIRE(buffer[1] == 2);
  }

  SECTION("Wraparound logic") {
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    bool overwritten = buffer.push_back(4); // Should overwrite 1

    REQUIRE(overwritten == true);
    REQUIRE(buffer.size() == 3);
    
    // Chronological order check
    REQUIRE(buffer[0] == 2);
    REQUIRE(buffer[1] == 3);
    REQUIRE(buffer[2] == 4);
  }

  SECTION("forEach iteration") {
    buffer.push_back(10);
    buffer.push_back(20);
    buffer.push_back(30);
    buffer.push_back(40); // buffer is now {20, 30, 40}

    std::vector<int> result;
    buffer.forEach([&](int val) { result.push_back(val); });

    std::vector<int> expected = {20, 30, 40};
    REQUIRE(result == expected);
  }
}
