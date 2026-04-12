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
  bool try_push_for(const T& item, std::chrono::duration<Rep, Period> timeout) {
    return push(item, timeout);
  }

  // Blocking push (move semantics) with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool try_push_for(T&& item, std::chrono::duration<Rep, Period> timeout) {
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
        if (!notFull_.wait_for(lock, timeout_val,
                               [this] { return queue_.size() < capacity_; }))
          return false;  // timeout
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

  // Blocking pop with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool try_pop_for(T& out, std::chrono::duration<Rep, Period> timeout) {
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
