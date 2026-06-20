/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <cmath>
#include <ebus/utils.hpp>

namespace {
// Helper to wrap the new buffer-based formatFloat for testing
std::string format(float value, int precision = 2,
                   float lower_threshold = 1e-6f,
                   float upper_threshold = 1e10f) {
  char buffer[64];
  ebus::formatFloat(value, precision, buffer, sizeof(buffer), lower_threshold,
                    upper_threshold);
  return std::string(buffer);
}
}  // namespace

TEST_CASE("formatFloat: Basic and Special values", "[utils][formatFloat]") {
  REQUIRE(format(0.0f) == "0");
  REQUIRE(format(1.234) == "1.23");
  REQUIRE(format(1.235) == "1.24");  // Rounding

  REQUIRE(format(std::nan("")) == "null");
  REQUIRE(format(INFINITY) == "inf");
  REQUIRE(format(-INFINITY) == "-inf");
}

TEST_CASE("formatFloat: Precision and Stripping", "[utils][formatFloat]") {
  // Fixed precision with trailing zero stripping
  REQUIRE(format(1.500, 3) == "1.5");
  REQUIRE(format(1.0, 2) == "1");

  // High precision
  REQUIRE(format(1.23456, 4) == "1.2346");
}

TEST_CASE("formatFloat: Thresholds and Scientific notation",
          "[utils][formatFloat]") {
  // Default lower threshold (1e-6)
  REQUIRE(format(0.000001f, 6) == "0.000001");
  REQUIRE(format(0.0000001f) == "1.00e-07");

  // Default upper threshold (1e10f). Note float precision for large numbers.
  REQUIRE(format(999999999.0f) == "1000000000");
  REQUIRE(format(10000000000.0f) == "1.00e+10");  // Exactly 1e10f

  SECTION("Custom thresholds") {
    // Switch to scientific much earlier
    REQUIRE(format(0.1, 2, 0.5) == "1.00e-01");

    // Switch to scientific at 100
    REQUIRE(format(150.0, 2, 1e-6, 100.0) == "1.50e+02");
  }
}

TEST_CASE("formatFloat: Input variation", "[utils][formatFloat]") {
  // Ensure negative values are handled correctly
  REQUIRE(format(-1.234) == "-1.23");
  REQUIRE(format(-0.0000001) == "-1.00e-07");
}
