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

#if defined(POSIX)
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace ebus {

template <typename T>
class QueuePosix {
 public:
  explicit QueuePosix(size_t size = 32) : capacity(size) {}

  // Blocking push with optional timeout (returns false on timeout/full)
  bool push(const T& item, std::chrono::milliseconds timeout =
                               std::chrono::milliseconds::max()) {
    std::unique_lock<std::mutex> lock(mutex);
    if (capacity > 0) {
      if (!notFull.wait_for(lock, timeout,
                            [this] { return queue.size() < capacity; }))
        return false;  // timeout
    }
    queue.push(item);
    notEmpty.notify_one();
    return true;
  }

  // Non-blocking push (returns false if full)
  bool try_push(const T& item) {
    std::lock_guard<std::mutex> lock(mutex);
    if (capacity > 0 && queue.size() >= capacity) return false;
    queue.push(item);
    notEmpty.notify_one();
    return true;
  }

  // Blocking pop with optional timeout (returns false on timeout/empty)
  bool pop(T& out, std::chrono::milliseconds timeout =
                       std::chrono::milliseconds::max()) {
    std::unique_lock<std::mutex> lock(mutex);
    if (!notEmpty.wait_for(lock, timeout, [this] { return !queue.empty(); }))
      return false;  // timeout
    out = queue.front();
    queue.pop();
    if (capacity > 0) notFull.notify_one();
    return true;
  }

  // Non-blocking pop (returns false if empty)
  bool try_pop(T& out) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) return false;
    out = queue.front();
    queue.pop();
    if (capacity > 0) notFull.notify_one();
    return true;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
  }

 private:
  std::queue<T> queue;
  mutable std::mutex mutex;
  std::condition_variable notEmpty;
  std::condition_variable notFull;
  size_t capacity;
};

}  // namespace ebus

#endif
