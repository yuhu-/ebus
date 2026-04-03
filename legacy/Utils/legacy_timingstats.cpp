/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cmath>
#include <ebus/Metrics.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

/**
 * Legacy-style test helper for TimingStats.
 */
void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

int main() {
  std::cout << "--- Test: RollingStats Math Engine ---" << std::endl;
  ebus::RollingStats stats;

  // Feed a simple dataset: 10, 20
  stats.addSample(10.0);
  stats.addSample(20.0);

  run_test("Count is 2", stats.getCount() == 2);
  run_test("Mean is 15.0", std::abs(stats.getMean() - 15.0) < 0.001);
  run_test("Last is 20.0", stats.getLast() == 20.0);

  // Variance calculation for {10, 20}:
  // Mean = 15
  // Sum of squares of differences = (10-15)^2 + (20-15)^2 = 25 + 25 = 50
  // Population Variance = 50 / 2 = 25
  // StdDev = sqrt(25) = 5.0
  run_test("StdDev is 5.0", std::abs(stats.getStdDev() - 5.0) < 0.001);

  std::cout << "--- Test: Reset Logic ---" << std::endl;
  stats.reset();
  run_test("Count is 0 after reset", stats.getCount() == 0);
  run_test("Mean is 0 after reset", stats.getMean() == 0.0);

  // Edge case: update with 0
  stats.addSample(0.0);
  run_test("Handles zero value",
           stats.getCount() == 1 && stats.getMean() == 0.0);

  std::cout << "\nAll TimingStats tests passed!" << std::endl;
  return EXIT_SUCCESS;
}