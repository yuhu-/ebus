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
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

namespace ebus {

template <typename T>
class QueuePosix {
 public:
  explicit QueuePosix(size_t capacity = 32) : capacity_(capacity) {}

  // Blocking push with optional timeout (returns false on timeout/full)
  bool push(const T& item, std::chrono::milliseconds timeout =
                               std::chrono::milliseconds::max()) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          notFull_.wait(lock, [this] { return queue_.size() < capacity_; });
        } else {
          if (!notFull_.wait_for(lock, timeout,
                                 [this] { return queue_.size() < capacity_; }))
            return false;  // timeout
        }
      }
      queue_.push(item);
    }
    notEmpty_.notify_one();
    return true;
  }

  // Blocking push (move semantics)
  bool push(T&& item, std::chrono::milliseconds timeout =
                          std::chrono::milliseconds::max()) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          notFull_.wait(lock, [this] { return queue_.size() < capacity_; });
        } else {
          if (!notFull_.wait_for(lock, timeout,
                                 [this] { return queue_.size() < capacity_; }))
            return false;  // timeout
        }
      }
      queue_.push(std::move(item));
    }
    notEmpty_.notify_one();
    return true;
  }

  // Non-blocking push (returns false if full)
  bool try_push(const T& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && queue_.size() >= capacity_) return false;
      queue_.push(item);
    }
    notEmpty_.notify_one();
    return true;
  }

  // Non-blocking push (move semantics)
  bool try_push(T&& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && queue_.size() >= capacity_) return false;
      queue_.push(std::move(item));
    }
    notEmpty_.notify_one();
    return true;
  }

  // Blocking pop with optional timeout (returns false on timeout/empty)
  bool pop(T& out, std::chrono::milliseconds timeout =
                       std::chrono::milliseconds::max()) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (timeout == std::chrono::milliseconds::max()) {
        notEmpty_.wait(lock, [this] { return !queue_.empty(); });
      } else {
        if (!notEmpty_.wait_for(lock, timeout,
                                [this] { return !queue_.empty(); }))
          return false;  // timeout
      }
      out = std::move(queue_.front());
      queue_.pop();
    }
    if (capacity_ > 0) notFull_.notify_one();
    return true;
  }

  // Non-blocking pop (returns false if empty)
  bool try_pop(T& out) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) return false;
      out = std::move(queue_.front());
      queue_.pop();
    }
    if (capacity_ > 0) notFull_.notify_one();
    return true;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<T> empty;
    std::swap(queue_, empty);
    if (capacity_ > 0) notFull_.notify_all();
  }

 private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  size_t capacity_;
};

}  // namespace ebus

#endif
