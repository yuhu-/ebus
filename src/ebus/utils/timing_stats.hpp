/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <ebus/metrics.hpp>
#include <mutex>

namespace ebus::detail {

/**
 * Specialized metric tracker for measuring durations between time points
 * and generic value samples.
 */
class TimingStats {
 public:
  TimingStats() : last_(0), max_(0), count_(0), marked_(false), begin_time_() {}
  virtual ~TimingStats() = default;

  inline void addSample(uint32_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    addSampleUnsynced(value);
  }

  inline void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_ = max_ = 0;
    count_ = 0;
  }

  inline MetricValues getValues() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {last_, max_, count_};
  }

  uint32_t getLast() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_;
  }

  uint64_t getCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
  }

  inline void markBegin(const Clock::time_point& begin = Clock::now()) {
    std::unique_lock<std::mutex> lock(mutex_);
    begin_time_ = begin;
    marked_ = true;
  }

  inline void markEnd(const Clock::time_point& end = Clock::now()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (marked_) {
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                          end - begin_time_)
                          .count();
      addSampleUnsynced(static_cast<uint32_t>(duration));
      marked_ = false;
    }
  }

  inline void addDurationWithTime(const Clock::time_point& begin,
                                  const Clock::time_point& end = Clock::now()) {
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
    addSample(static_cast<uint32_t>(duration));
  }

 protected:
  void addSampleUnsynced(uint32_t value) {
    last_ = value;
    if (value > max_) max_ = value;
    ++count_;
  }

  mutable std::mutex mutex_;
  uint32_t last_;
  uint32_t max_;
  uint64_t count_;

 private:
  bool marked_;
  Clock::time_point begin_time_;
};

}  // namespace ebus::detail