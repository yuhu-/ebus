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
    : last(0),
      count(0),
      mean(0),
      m2(0),
      is_marked(false),
      begin_time(std::chrono::steady_clock::time_point()) {}

void TimingStats::markBegin(
    const std::chrono::steady_clock::time_point& begin) {
  begin_time = begin;
  is_marked = true;
}

void TimingStats::markEnd(const std::chrono::steady_clock::time_point& end) {
  if (is_marked) {
    int64_t duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin_time)
            .count();
    calc(duration);
    is_marked = false;
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
  values.last = last;
  values.mean = mean;
  values.stddev = stddev();
  values.count = count;
  return values;
}

void TimingStats::clear() {
  last = 0;
  count = 0;
  mean = 0;
  m2 = 0;
  is_marked = false;
  begin_time = std::chrono::steady_clock::time_point();
}

void TimingStats::calc(double value) {
  last = value;
  ++count;
  double delta = value - mean;
  mean += delta / count;
  double delta2 = value - mean;
  m2 += delta * delta2;
}

double TimingStats::variance() const {
  return count > 1 ? m2 / (count - 1) : 0;
}

double TimingStats::stddev() const { return sqrt(variance()); }

}  // namespace ebus
