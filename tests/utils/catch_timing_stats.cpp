/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <cmath>

#include "utils/timing_stats.hpp"

using namespace ebus::detail;

TEST_CASE("TimingStats: Basic statistics", "[utils][timingstats]") {
  RollingStats stats;

  stats.addSample(10.0);
  stats.addSample(20.0);

  REQUIRE(stats.getCount() == 2);
  REQUIRE(std::abs(stats.getMean() - 15.0) < 0.001);
  REQUIRE(stats.getLast() == 20.0);

  // Population stddev for {10,20} is 5.0
  REQUIRE(std::abs(stats.getStdDev() - 5.0) < 0.001);
}

TEST_CASE("TimingStats: Reset and edge cases", "[utils][timingstats]") {
  RollingStats stats;
  stats.addSample(1.0);
  stats.reset();

  REQUIRE(stats.getCount() == 0);
  REQUIRE(stats.getMean() == 0.0);

  stats.addSample(0.0);
  REQUIRE(stats.getCount() == 1);
  REQUIRE(stats.getMean() == 0.0);
}