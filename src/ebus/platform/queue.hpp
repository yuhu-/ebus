/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ebus::detail::platform {

/**
 * @brief A thread-safe, blocking Queue implementation using standard C++ libraries.
 * Fully compatible with move-only types and runs on both POSIX and FreeRTOS (ESP-IDF) platforms.
 */
template <typename T>
class Queue {
 public:
  explicit Queue(size_t capacity = 32)
      : buffer_(capacity),
        capacity_(capacity),
        head_(0),
        tail_(0),
        size_(0),
        shutdown_(false) {
    if (capacity == 0)
      throw std::invalid_argument("Queue capacity must be > 0");
  }

  ~Queue() = default;

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

  // Blocking push with timeout in milliseconds
  bool push(const T& item, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout =
          (timeout_ms == 0xffffffff) ? std::chrono::milliseconds::max()
                                     : std::chrono::milliseconds(timeout_ms);

      std::unique_lock<std::mutex> lock(mutex_);
      if (shutdown_) return false;
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          not_full_.wait(lock,
                         [this] { return size_ < capacity_ || shutdown_; });
        } else {
          if (!not_full_.wait_for(lock, timeout, [this] {
                return size_ < capacity_ || shutdown_;
              }))
            return false;  // timeout
        }
      }
      if (shutdown_) return false;
      buffer_[tail_] = item;
      tail_ = (tail_ + 1) % capacity_;
      size_++;
    }
    not_empty_.notify_one();
    return true;
  }

  // Blocking push (move semantics) with timeout in milliseconds
  bool push(T&& item, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout =
          (timeout_ms == 0xffffffff) ? std::chrono::milliseconds::max()
                                     : std::chrono::milliseconds(timeout_ms);

      std::unique_lock<std::mutex> lock(mutex_);
      if (shutdown_) return false;
      if (capacity_ > 0) {
        if (timeout == std::chrono::milliseconds::max()) {
          not_full_.wait(lock,
                         [this] { return size_ < capacity_ || shutdown_; });
        } else {
          if (!not_full_.wait_for(lock, timeout, [this] {
                return size_ < capacity_ || shutdown_;
              }))
            return false;  // timeout
        }
      }
      if (shutdown_) return false;
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
    auto dur_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    uint32_t ms_val = (dur_ms.count() < 0) ? 0xffffffff : static_cast<uint32_t>(dur_ms.count());
    return push(std::move(item), ms_val);
  }

  // Non-blocking push (returns false if full)
  bool tryPush(const T& item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (shutdown_ || (capacity_ > 0 && size_ >= capacity_)) return false;
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
      if (shutdown_ || (capacity_ > 0 && size_ >= capacity_)) return false;
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

  // Blocking pop with timeout in milliseconds
  bool pop(T& out, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout =
          (timeout_ms == 0xffffffff) ? std::chrono::milliseconds::max()
                                     : std::chrono::milliseconds(timeout_ms);
      std::unique_lock<std::mutex> lock(mutex_);
      if (shutdown_ && size_ == 0) return false;
      if (timeout == std::chrono::milliseconds::max()) {
        not_empty_.wait(lock, [this] { return size_ > 0 || shutdown_; });
      } else {
        if (!not_empty_.wait_for(lock, timeout,
                                 [this] { return size_ > 0 || shutdown_; }))
          return false;  // timeout
      }
      if (size_ == 0) return false;
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
      if (size_ == 0 || shutdown_) return false;
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

  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  bool isShutdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    if (capacity_ > 0) not_full_.notify_all();
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    shutdown_ = false;
    not_full_.notify_all();
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
  bool shutdown_;
};

}  // namespace ebus::detail::platform
