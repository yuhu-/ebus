/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(POSIX)
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "platform/mutex.hpp"

namespace ebus::detail::platform {

template <typename T>
class QueuePosix {
 public:
  explicit QueuePosix(size_t capacity = 32)
      : buffer_(capacity),
        capacity_(capacity),
        head_(0),
        tail_(0),
        size_(0),
        shutdown_(false) {
    if (capacity == 0)
      throw std::invalid_argument("Queue capacity must be > 0");
  }

  // Explicitly delete copy operations
  QueuePosix(const QueuePosix&) = delete;
  QueuePosix& operator=(const QueuePosix&) = delete;

  // Move constructor
  QueuePosix(QueuePosix&& other) noexcept {
    platform::LockGuard<platform::Mutex> lock(other.mutex_);
    buffer_ = std::move(other.buffer_);
    capacity_ = other.capacity_;
    head_ = other.head_;
    tail_ = other.tail_;
    size_ = other.size_;
    shutdown_ = other.shutdown_;
  }

  // Move assignment
  QueuePosix& operator=(QueuePosix&& other) noexcept {
    if (this != &other) {
      platform::UniqueLock<platform::Mutex> lock_this(mutex_, std::defer_lock);
      platform::UniqueLock<platform::Mutex> lock_other(other.mutex_,
                                                       std::defer_lock);
      std::lock(lock_this, lock_other);
      buffer_ = std::move(other.buffer_);
      capacity_ = other.capacity_;
      head_ = other.head_;
      tail_ = other.tail_;
      size_ = other.size_;
      shutdown_ = other.shutdown_;
    }
    return *this;
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

      platform::UniqueLock<platform::Mutex> lock(mutex_);
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

  // Blocking push (move semantics) with timeout in milliseconds (Unified API)
  bool push(T&& item, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout;
      if (timeout_ms == 0xffffffff) {
        timeout = std::chrono::milliseconds::max();
      } else {
        timeout = std::chrono::milliseconds(timeout_ms);
      }

      platform::UniqueLock<platform::Mutex> lock(mutex_);
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
    {
      auto dur_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
      std::chrono::milliseconds timeout_val;
      if (dur_ms.count() < 0) {
        timeout_val = std::chrono::milliseconds::max();
      } else {
        timeout_val = dur_ms;
      }
      platform::UniqueLock<platform::Mutex> lock(mutex_);
      if (shutdown_) return false;
      if (capacity_ > 0) {
        if (!not_full_.wait_for(lock, timeout_val, [this] {
              return size_ < capacity_ || shutdown_;
            }))
          return false;  // timeout
      }
      if (shutdown_) return false;
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
      platform::LockGuard<platform::Mutex> lock(mutex_);
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
      platform::LockGuard<platform::Mutex> lock(mutex_);
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

  // Blocking pop with timeout in milliseconds (Unified API)
  bool pop(T& out, uint32_t timeout_ms = 0xffffffff) {
    {
      std::chrono::milliseconds timeout =
          (timeout_ms == 0xffffffff) ? std::chrono::milliseconds::max()
                                     : std::chrono::milliseconds(timeout_ms);
      platform::UniqueLock<platform::Mutex> lock(mutex_);
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
      platform::LockGuard<platform::Mutex> lock(mutex_);
      if (size_ == 0 || shutdown_) return false;
      out = std::move(buffer_[head_]);
      head_ = (head_ + 1) % capacity_;
      size_--;
    }
    if (capacity_ > 0) not_full_.notify_one();
    return true;
  }

  void shutdown() {
    {
      platform::LockGuard<platform::Mutex> lock(mutex_);
      shutdown_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  bool isShutdown() const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return shutdown_;
  }

  void clear() {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    if (capacity_ > 0) not_full_.notify_all();
  }

  void reset() {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    shutdown_ = false;
    not_full_.notify_all();
  }

  size_t size() const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return size_;
  }
  bool empty() const { return size_ == 0; }

 private:
  std::vector<T> buffer_;
  mutable platform::Mutex mutex_;
  platform::ConditionVariable not_empty_;
  platform::ConditionVariable not_full_;
  size_t capacity_;
  size_t head_;
  size_t tail_;
  size_t size_;
  bool shutdown_;
};

}  // namespace ebus::detail::platform

#endif  // POSIX
