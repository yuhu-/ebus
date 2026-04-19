/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "utils/timing_stats.hpp"

#include <cmath>

ebus::RollingStats::RollingStats()
    : last_(0.0), min_(0.0), max_(0.0), count_(0), mean_(0.0), m2_(0.0) {}

void ebus::RollingStats::addSample(double value) {
  last_ = value;
  if (count_ == 0) {
    min_ = value;
    max_ = value;
  } else {
    if (value < min_) min_ = value;
    if (value > max_) max_ = value;
  }

  ++count_;
  double delta = value - mean_;
  mean_ += delta / count_;
  double delta2 = value - mean_;
  m2_ += delta * delta2;
}

void ebus::RollingStats::reset() {
  last_ = min_ = max_ = mean_ = m2_ = 0.0;
  count_ = 0;
}

ebus::MetricValues ebus::RollingStats::getValues() const {
  return {last_, min_, max_, mean_, getStdDev(), count_};
}

double ebus::RollingStats::getStdDev() const {
  if (count_ == 0) return 0.0;
  return std::sqrt(m2_ / count_);
}

ebus::TimingStats::TimingStats()
    : RollingStats(),
      marked_(false),
      begin_time_(std::chrono::steady_clock::time_point()) {}

void ebus::TimingStats::markBegin(
    const std::chrono::steady_clock::time_point& begin) {
  begin_time_ = begin;
  marked_ = true;
}

void ebus::TimingStats::markEnd(
    const std::chrono::steady_clock::time_point& end) {
  if (marked_) {
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin_time_)
            .count();
    addSample(static_cast<double>(duration));
    marked_ = false;
  }
}

void ebus::TimingStats::addDurationWithTime(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end) {
  int64_t duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  addSample(static_cast<double>(duration));
}
