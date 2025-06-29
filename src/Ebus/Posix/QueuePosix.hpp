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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace ebus {

template <typename T>
class QueuePosix {
 public:
  explicit QueuePosix(size_t capacity = 0) : m_capacity(capacity) {}

  // Blocking push with optional timeout (returns false on timeout/full)
  bool push(const T& item, std::chrono::milliseconds timeout =
                               std::chrono::milliseconds::max()) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_capacity > 0) {
      if (!m_not_full.wait_for(lock, timeout,
                               [this] { return m_queue.size() < m_capacity; }))
        return false;  // timeout
    }
    m_queue.push(item);
    m_not_empty.notify_one();
    return true;
  }

  // Non-blocking push (returns false if full)
  bool try_push(const T& item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_capacity > 0 && m_queue.size() >= m_capacity) return false;
    m_queue.push(item);
    m_not_empty.notify_one();
    return true;
  }

  // Blocking pop with optional timeout (returns false on timeout/empty)
  bool pop(T& out, std::chrono::milliseconds timeout =
                       std::chrono::milliseconds::max()) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_not_empty.wait_for(lock, timeout,
                              [this] { return !m_queue.empty(); }))
      return false;  // timeout
    out = m_queue.front();
    m_queue.pop();
    if (m_capacity > 0) m_not_full.notify_one();
    return true;
  }

  // Non-blocking pop (returns false if empty)
  bool try_pop(T& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) return false;
    out = m_queue.front();
    m_queue.pop();
    if (m_capacity > 0) m_not_full.notify_one();
    return true;
  }

  const size_t size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

  const size_t capacity() const { return m_capacity; }

 private:
  std::queue<T> m_queue;
  mutable std::mutex m_mutex;
  std::condition_variable m_not_empty;
  std::condition_variable m_not_full;
  size_t m_capacity;  // 0 = unlimited
};

}  // namespace ebus
