/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/metrics.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

/**
 * Specialized metric tracker for measuring durations between time points
 * and generic value samples.
 */
class TimingStats {
 public:
  // Lifecycle
  TimingStats()
      : last_(0),
        max_(0),
        sum_(0),
        count_(0),
        marked_(false),
        begin_time_us_(0) {}
  virtual ~TimingStats() = default;

  // Working Methods
  inline void addSample(uint32_t value) {
    last_.store(value, std::memory_order_relaxed);
    sum_.fetch_add(value, std::memory_order_relaxed);
    updateMaxAtomic(max_, value);
    count_.fetch_add(1, std::memory_order_relaxed);
  }

  inline void reset() {
    last_.store(0, std::memory_order_relaxed);
    max_.store(0, std::memory_order_relaxed);
    sum_.store(0, std::memory_order_relaxed);
    count_.store(0, std::memory_order_relaxed);
    marked_.store(false, std::memory_order_relaxed);
  }

  inline void markBegin(const Clock::time_point& begin = Clock::now()) {
    begin_time_us_.store(std::chrono::duration_cast<std::chrono::microseconds>(
                             begin.time_since_epoch())
                             .count(),
                         std::memory_order_relaxed);
    marked_.store(true, std::memory_order_release);
  }

  inline void markEnd(const Clock::time_point& end = Clock::now()) {
    if (marked_.load(std::memory_order_acquire)) {
      uint64_t end_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            end.time_since_epoch())
                            .count();
      uint64_t duration =
          end_us - begin_time_us_.load(std::memory_order_relaxed);
      addSample(static_cast<uint32_t>(duration));
      marked_.store(false, std::memory_order_release);
    }
  }

  inline void addDurationWithTime(const Clock::time_point& begin,
                                  const Clock::time_point& end = Clock::now()) {
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();
    addSample(static_cast<uint32_t>(duration));
  }

  // Status/Telemetry
  inline MetricValues getValues() const {
    return {last_.load(std::memory_order_relaxed),
            max_.load(std::memory_order_relaxed),
            sum_.load(std::memory_order_relaxed),
            count_.load(std::memory_order_relaxed)};
  }
  uint32_t getLast() const { return last_.load(std::memory_order_relaxed); }
  uint64_t getCount() const { return count_.load(std::memory_order_relaxed); }

 private:
  std::atomic<uint32_t> last_{0};
  std::atomic<uint32_t> max_{0};
  std::atomic<uint64_t> sum_{0};
  std::atomic<uint64_t> count_{0};
  std::atomic<bool> marked_{false};
  std::atomic<uint64_t> begin_time_us_{0};
};

}  // namespace ebus::detail