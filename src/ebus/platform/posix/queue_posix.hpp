/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
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

  // Blocking push with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPushFor(const T& item, std::chrono::duration<Rep, Period> timeout) {
    return push(item, timeout);
  }

  // Blocking push (move semantics) with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPushFor(T&& item, std::chrono::duration<Rep, Period> timeout) {
    return push(std::move(item), timeout);
  }

  // Blocking push with duration
  template <typename Rep, typename Period>
  bool push(const T& item, std::chrono::duration<Rep, Period> timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    return push(item, static_cast<uint32_t>(ms.count()));
  }

  // Blocking push with timeout in milliseconds (Unified API)
  bool push(const T& item, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout;
      if (timeout_ms == 0xffffffff) {
        timeout = std::chrono::milliseconds::max();
      } else {
        timeout = std::chrono::milliseconds(timeout_ms);
      }

      std::unique_lock<std::mutex> lock(mutex_);
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        } else {
          if (!not_full_.wait_for(lock, timeout,
                                  [this] { return queue_.size() < capacity_; }))
            return false;  // timeout
        }
      }
      queue_.push(item);
    }
    not_empty_.notify_one();
    return true;
  }

  // Blocking push (move semantics) with timeout in milliseconds (Unified API)
  bool push(T&& item, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout;
      if (timeout_ms == 0xffffffff) {
        timeout = std::chrono::milliseconds::max();
      } else {
        timeout = std::chrono::milliseconds(timeout_ms);
      }

      std::unique_lock<std::mutex> lock(mutex_);
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        } else {
          if (!not_full_.wait_for(lock, timeout,
                                  [this] { return queue_.size() < capacity_; }))
            return false;  // timeout
        }
      }
      queue_.push(std::move(item));
    }
    not_empty_.notify_one();
    return true;
  }

  // Blocking push (move semantics) with duration
  template <typename Rep, typename Period>
  bool push(T&& item, std::chrono::duration<Rep, Period> timeout) {
    {
      auto dur_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
      std::chrono::milliseconds timeout_val;
      if (dur_ms.count() < 0) {
        timeout_val = std::chrono::milliseconds::max();
      } else {
        timeout_val = dur_ms;
      }
      std::unique_lock<std::mutex> lock(mutex_);
      if (capacity_ > 0) {
        if (!not_full_.wait_for(lock, timeout_val,
                                [this] { return queue_.size() < capacity_; }))
          return false;  // timeout
      }
      queue_.push(std::move(item));
    }
    not_empty_.notify_one();
    return true;
  }

  // Non-blocking push (returns false if full)
  bool tryPush(const T& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && queue_.size() >= capacity_) return false;
      queue_.push(item);
    }
    not_empty_.notify_one();
    return true;
  }

  // Non-blocking push (move semantics)
  bool tryPush(T&& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && queue_.size() >= capacity_) return false;
      queue_.push(std::move(item));
    }
    not_empty_.notify_one();
    return true;
  }

  // Blocking pop with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPopFor(T& out, std::chrono::duration<Rep, Period> timeout) {
    return pop(out, timeout);
  }

  // Blocking pop with duration
  template <typename Rep, typename Period>
  bool pop(T& out, std::chrono::duration<Rep, Period> timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    return pop(out, static_cast<uint32_t>(ms.count()));
  }

  // Blocking pop with timeout in milliseconds (Unified API)
  bool pop(T& out, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout =
          (timeout_ms == 0xffffffff) ? std::chrono::milliseconds::max()
                                     : std::chrono::milliseconds(timeout_ms);
      std::unique_lock<std::mutex> lock(mutex_);
      if (timeout == std::chrono::milliseconds::max()) {
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
      } else {
        if (!not_empty_.wait_for(lock, timeout,
                                 [this] { return !queue_.empty(); }))
          return false;  // timeout
      }
      out = std::move(queue_.front());
      queue_.pop();
    }
    if (capacity_ > 0) not_full_.notify_one();
    return true;
  }

  // Non-blocking pop (returns false if empty)
  bool tryPop(T& out) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) return false;
      out = std::move(queue_.front());
      queue_.pop();
    }
    if (capacity_ > 0) not_full_.notify_one();
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
    if (capacity_ > 0) not_full_.notify_all();
  }

 private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  size_t capacity_;
};

}  // namespace ebus

#endif
