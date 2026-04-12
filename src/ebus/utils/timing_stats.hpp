/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <ebus/metrics.hpp>

namespace ebus {

/**
 * Specialized metric tracker for measuring durations between time points.
 */
class TimingStats : public RollingStats {
 public:
  TimingStats();

  void markBegin(const std::chrono::steady_clock::time_point& begin =
                     std::chrono::steady_clock::now());

  void markEnd(const std::chrono::steady_clock::time_point& end =
                   std::chrono::steady_clock::now());

  void addDurationWithTime(const std::chrono::steady_clock::time_point& begin,
                           const std::chrono::steady_clock::time_point& end =
                               std::chrono::steady_clock::now());

 private:
  bool marked_;
  std::chrono::steady_clock::time_point beginTime_;
};

}  // namespace ebus
