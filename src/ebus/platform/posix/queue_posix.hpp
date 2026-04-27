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
#include <stdexcept>
#include <utility>
#include <vector>

namespace ebus::detail::platform {

template <typename T>
class QueuePosix {
 public:
  explicit QueuePosix(size_t capacity = 32)
      : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), size_(0) {
    if (capacity == 0)
      throw std::invalid_argument("Queue capacity must be > 0");
  }

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
          not_full_.wait(lock, [this] { return size_ < capacity_; });
        } else {
          if (!not_full_.wait_for(lock, timeout,
                                  [this] { return size_ < capacity_; }))
            return false;  // timeout
        }
      }
      buffer_[tail_] = item;
      tail_ = (tail_ + 1) % capacity_;
      size_++;
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
          not_full_.wait(lock, [this] { return size_ < capacity_; });
        } else {
          if (!not_full_.wait_for(lock, timeout,
                                  [this] { return size_ < capacity_; }))
            return false;  // timeout
        }
      }
      buffer_[tail_] = std::move(item);
      tail_ = (tail_ + 1) % capacity_;
      size_++;
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
                                [this] { return size_ < capacity_; }))
          return false;  // timeout
      }
      buffer_[tail_] = std::move(item);
      tail_ = (tail_ + 1) % capacity_;
      size_++;
    }
    not_empty_.notify_one();
    return true;
  }

  // Non-blocking push (returns false if full)
  bool tryPush(const T& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && size_ >= capacity_) return false;
      buffer_[tail_] = item;
      tail_ = (tail_ + 1) % capacity_;
      size_++;
    }
    not_empty_.notify_one();
    return true;
  }

  // Non-blocking push (move semantics)
  bool tryPush(T&& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (capacity_ > 0 && size_ >= capacity_) return false;
      buffer_[tail_] = std::move(item);
      tail_ = (tail_ + 1) % capacity_;
      size_++;
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
        not_empty_.wait(lock, [this] { return size_ > 0; });
      } else {
        if (!not_empty_.wait_for(lock, timeout, [this] { return size_ > 0; }))
          return false;  // timeout
      }
      out = std::move(buffer_[head_]);
      head_ = (head_ + 1) % capacity_;
      size_--;
    }
    if (capacity_ > 0) not_full_.notify_one();
    return true;
  }

  // Non-blocking pop (returns false if empty)
  bool tryPop(T& out) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (size_ == 0) return false;
      out = std::move(buffer_[head_]);
      head_ = (head_ + 1) % capacity_;
      size_--;
    }
    if (capacity_ > 0) not_full_.notify_one();
    return true;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    if (capacity_ > 0) not_full_.notify_all();
  }

 private:
  std::vector<T> buffer_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  size_t capacity_;
  size_t head_;
  size_t tail_;
  size_t size_;
};

}  // namespace ebus::detail::platform

#endif  // POSIX
