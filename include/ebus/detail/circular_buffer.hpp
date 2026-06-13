/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <utility>

#include "platform/mutex.hpp"

namespace ebus::detail {

/**
 * A simple circular buffer utility using a fixed-size internal array.
 * Provides chronological access and zero-allocation updates once at capacity.
 */
template <typename T, size_t Cap>
class CircularBuffer {
 public:
  // Lifecycle
  CircularBuffer() : head_(0), size_(0) {}

  // Working Methods
  // Pushes an item into the buffer. Returns true if an old element was
  // overwritten.
  bool push_back(const T& item) {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return pushImpl(item);
  }

  bool push_back(T&& item) {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return pushImpl(std::move(item));
  }

  /**
   * @brief Attempts to remove and return the oldest element.
   * @param out Reference to store the popped item.
   * @return true if an item was popped, false if empty.
   */
  bool tryPop(T& out) {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (size_ == 0) return false;
    out = std::move(buffer_[head_]);
    head_ = (head_ + 1) % Cap;
    size_--;
    return true;
  }

  void clear() {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    size_ = 0;
    head_ = 0;
  }

  /**
   * @brief Iterates over the buffer's contents in chronological order.
   * @param callback Function to invoke for each item.
   */
  template <typename Func>
  void forEach(Func&& callback) const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    for (size_t i = 0; i < size_; ++i) {
      callback(buffer_[(head_ + i) % Cap]);
    }
  }

  // Status/Telemetry
  size_t size() const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return size_;
  }
  static constexpr size_t capacity() noexcept { return Cap; }
  bool empty() const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    return size_ == 0;
  }

  T operator[](size_t index) const {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (index >= size_) return T();
    return buffer_[(head_ + index) % Cap];
  }

 private:
  mutable platform::Mutex mutex_;
  T buffer_[Cap];
  size_t head_ = 0;
  size_t size_ = 0;

  template <typename U>
  bool pushImpl(U&& item) {
    bool overwritten = false;
    if (size_ == Cap) {
      buffer_[head_] = std::forward<U>(item);
      head_ = (head_ + 1) % Cap;
      overwritten = true;
    } else {
      buffer_[(head_ + size_) % Cap] = std::forward<U>(item);
      size_++;
    }
    return overwritten;
  }
};

}  // namespace ebus::detail
