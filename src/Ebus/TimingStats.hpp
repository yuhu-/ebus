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

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>

namespace ebus {

class TimingStats {
 public:
  struct Values {
    double last;
    double mean;
    double stddev;
    uint64_t count;
  };

  TimingStats();

  void markBegin(const std::chrono::steady_clock::time_point& begin =
                     std::chrono::steady_clock::now());

  void markEnd(const std::chrono::steady_clock::time_point& end =
                   std::chrono::steady_clock::now());

  void addDuration(double value);

  void addDurationWithTime(const std::chrono::steady_clock::time_point& begin,
                           const std::chrono::steady_clock::time_point& end =
                               std::chrono::steady_clock::now());

  Values getValues() const;

  void clear();

 private:
  double last;
  uint64_t count;
  double mean;
  double m2;
  bool is_marked;
  std::chrono::steady_clock::time_point begin_time;

  void calc(double value);
  double variance() const;
  double stddev() const;
};

}  // namespace ebus
