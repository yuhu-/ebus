/*
 * Copyright (C) 2025 Roland Jax
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

#pragma once

#include <cmath>
#include <cstdint>

namespace ebus {

// Timing statistics for measuring durations (in microseconds).
struct TimingStats {
  double last = 0;  // the most recently added value
  uint64_t count = 0;
  double mean = 0;
  double m2 = 0;  // for variance

  void add(double x) {
    last = x;
    ++count;
    double delta = x - mean;
    mean += delta / count;
    double delta2 = x - mean;
    m2 += delta * delta2;
  }

  double variance() const { return count > 1 ? m2 / (count - 1) : 0; }

  double stddev() const { return sqrt(variance()); }

  void clear() {
    last = 0;
    count = 0;
    mean = 0;
    m2 = 0;
  }
};

}  // namespace ebus
