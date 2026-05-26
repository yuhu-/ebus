/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <cmath>

#include "utils/timing_stats.hpp"

using namespace ebus::detail;

TEST_CASE("TimingStats: Basic statistics", "[utils][timingstats]") {
  TimingStats stats;

  stats.addSample(10);
  stats.addSample(20);

  REQUIRE(stats.getCount() == 2);
  REQUIRE(stats.getLast() == 20);
  REQUIRE(stats.getValues().max_us == 20);
}

TEST_CASE("TimingStats: Reset and edge cases", "[utils][timingstats]") {
  TimingStats stats;
  stats.addSample(1);
  stats.reset();

  REQUIRE(stats.getCount() == 0);

  stats.addSample(0);
  REQUIRE(stats.getCount() == 1);
}