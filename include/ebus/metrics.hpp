/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus {

/**
 * Results of a rolling metric calculation.
 */
struct MetricValues {
  double last = 0.0;
  double min = 0.0;
  double max = 0.0;
  double mean = 0.0;
  double stddev = 0.0;
  uint64_t count = 0;
};

/**
 * Math engine for online statistics calculation using Welford's algorithm.
 * This class is platform-independent and does not depend on chrono.
 */
class RollingStats {
 public:
  RollingStats();
  virtual ~RollingStats() = default;

  /**
   * Adds a new data point to the dataset and updates rolling statistics.
   */
  void addSample(double value);

  /**
   * Resets all accumulated data.
   */
  void reset();

  MetricValues getValues() const;

  /**
   * Returns the sum of all samples added.
   */
  double getSum() const { return mean_ * count_; }

  double getLast() const { return last_; }
  double getMean() const { return mean_; }
  uint64_t getCount() const { return count_; }
  double getStdDev() const;

 protected:
  double last_;
  double min_;
  double max_;
  uint64_t count_;
  double mean_;
  double m2_;  // Internal state for variance calculation
};

}  // namespace ebus