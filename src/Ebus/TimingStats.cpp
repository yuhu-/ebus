/*
 * Copyright (C) 2025-2026 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include "TimingStats.hpp"

namespace ebus {

TimingStats::TimingStats()
    : last_(0),
      count_(0),
      mean_(0),
      m2_(0),
      marked_(false),
      beginTime_(std::chrono::steady_clock::time_point()) {}

void TimingStats::markBegin(
    const std::chrono::steady_clock::time_point& begin) {
  beginTime_ = begin;
  marked_ = true;
}

void TimingStats::markEnd(const std::chrono::steady_clock::time_point& end) {
  if (marked_) {
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - beginTime_)
            .count();
    calc(duration);
    marked_ = false;
  }
}

void TimingStats::addDuration(double value) { calc(value); }

void TimingStats::addDurationWithTime(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end) {
  int64_t duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  calc(duration);
}

TimingStats::Values TimingStats::getValues() const {
  Values values;
  values.last = last_;
  values.mean = mean_;
  values.stddev = stddev();
  values.count = count_;
  return values;
}

void TimingStats::clear() {
  last_ = 0;
  count_ = 0;
  mean_ = 0;
  m2_ = 0;
  marked_ = false;
  beginTime_ = std::chrono::steady_clock::time_point();
}

void TimingStats::calc(double value) {
  last_ = value;
  ++count_;
  double delta = value - mean_;
  mean_ += delta / count_;
  double delta2 = value - mean_;
  m2_ += delta * delta2;
}

double TimingStats::variance() const {
  return count_ > 1 ? m2_ / (count_ - 1) : 0;
}

double TimingStats::stddev() const { return sqrt(variance()); }

}  // namespace ebus
