/*
 * Copyright (C) Roland Jax 2012-2024 <roland.jax@liwest.at>
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

#ifndef SRC_NQUEUE_H_
#define SRC_NQUEUE_H_

#include <stddef.h>

#include <condition_variable>
#include <mutex>
#include <queue>

namespace ebus {

template <typename T>
class NQueue {
 public:
  NQueue() : m_queue(), m_mutex(), m_condition() {}

  void enqueue(T item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(item);
    m_condition.notify_one();
  }

  T dequeue() {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty()) m_condition.wait(lock);

    T val = m_queue.front();
    m_queue.pop();
    return val;
  }

  size_t size() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

 private:
  std::queue<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_condition;
};

}  // namespace ebus

#endif  // SRC_NQUEUE_H_
