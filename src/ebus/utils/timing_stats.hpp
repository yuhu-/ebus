/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ebus/metrics.hpp>
#include <mutex>

namespace ebus::detail {

/**
 * Math engine for online statistics calculation using Welford's algorithm.
 */
class RollingStats {
 public:
  RollingStats()
      : last_(0.0f),
        min_(0.0f),
        max_(0.0f),
        count_(0),
        mean_(0.0f),
        m2_(0.0f) {}
  virtual ~RollingStats() = default;

  inline void addSample(float value) {
    std::lock_guard<std::mutex> lock(mutex_);
    addSampleUnsynced(value);
  }

  inline void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_ = min_ = max_ = mean_ = m2_ = 0.0f;
    count_ = 0;
  }

  inline MetricValues getValues() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {last_, min_, max_, mean_, static_cast<float>(getStdDev()), count_};
  }

  float getSum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mean_ * count_;
  }

  float getLast() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_;
  }

  float getMean() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mean_;
  }

  uint32_t getCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
  }

  inline float getStdDev() const {
    // Note: Caller in getValues already holds lock, but direct calls need it.
    return (count_ == 0) ? 0.0f : std::sqrt(m2_ / static_cast<float>(count_));
  }

 protected:
  void addSampleUnsynced(float value) {
    last_ = value;
    if (count_ == 0) {
      min_ = value;
      max_ = value;
    } else {
      if (value < min_) min_ = value;
      if (value > max_) max_ = value;
    }
    ++count_;
    float delta = value - mean_;
    mean_ += delta / count_;
    float delta2 = value - mean_;
    m2_ += delta * delta2;
  }

  mutable std::mutex mutex_;
  float last_;
  float min_;
  float max_;
  uint32_t count_;
  float mean_;
  float m2_;
};

/**
 * Specialized metric tracker for measuring durations between time points.
 */
class TimingStats : public RollingStats {
 public:
  TimingStats() : RollingStats(), marked_(false), begin_time_() {}

  inline void markBegin(const Clock::time_point& begin =
                            Clock::now()) {
    std::unique_lock<std::mutex> lock(mutex_);
    begin_time_ = begin;
    marked_ = true;
  }

  inline void markEnd(const Clock::time_point& end =
                          Clock::now()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (marked_) {
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end - begin_time_)
                          .count();
      addSampleUnsynced(static_cast<float>(duration));
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
    addSample(static_cast<float>(duration));
  }

 private:
  bool marked_;
  std::chrono::steady_clock::time_point begin_time_;
};

}  // namespace ebus::detail