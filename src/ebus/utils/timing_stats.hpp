/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ebus/metrics.hpp>

namespace ebus::detail {

/**
 * Math engine for online statistics calculation using Welford's algorithm.
 */
class RollingStats {
 public:
  RollingStats()
      : last_(0.0), min_(0.0), max_(0.0), count_(0), mean_(0.0), m2_(0.0) {}
  virtual ~RollingStats() = default;

  inline void addSample(double value) {
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

  inline void reset() {
    last_ = min_ = max_ = mean_ = m2_ = 0.0;
    count_ = 0;
  }

  inline MetricValues getValues() const {
    return {last_, min_, max_, mean_, getStdDev(), count_};
  }

  double getSum() const { return mean_ * count_; }
  double getLast() const { return last_; }
  double getMean() const { return mean_; }
  uint64_t getCount() const { return count_; }
  inline double getStdDev() const {
    return (count_ == 0) ? 0.0 : std::sqrt(m2_ / count_);
  }

 protected:
  double last_;
  double min_;
  double max_;
  uint64_t count_;
  double mean_;
  double m2_;
};

/**
 * Specialized metric tracker for measuring durations between time points.
 */
class TimingStats : public RollingStats {
 public:
  TimingStats() : RollingStats(), marked_(false), begin_time_() {}

  inline void markBegin(const std::chrono::steady_clock::time_point& begin =
                            std::chrono::steady_clock::now()) {
    begin_time_ = begin;
    marked_ = true;
  }

  inline void markEnd(const std::chrono::steady_clock::time_point& end =
                          std::chrono::steady_clock::now()) {
    if (marked_) {
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end - begin_time_)
                          .count();
      addSample(static_cast<double>(duration));
      marked_ = false;
    }
  }

  inline void addDurationWithTime(
      const std::chrono::steady_clock::time_point& begin,
      const std::chrono::steady_clock::time_point& end =
          std::chrono::steady_clock::now()) {
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
    addSample(static_cast<double>(duration));
  }

 private:
  bool marked_;
  std::chrono::steady_clock::time_point begin_time_;
};

}  // namespace ebus::detail