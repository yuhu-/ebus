/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <mutex>
#include <utility>

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
    std::lock_guard<std::mutex> lock(mutex_);
    return push_impl(item);
  }

  bool push_back(T&& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    return push_impl(std::move(item));
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_ = 0;
    head_ = 0;
  }

  /**
   * @brief Iterates over the buffer's contents in chronological order.
   * @param callback Function to invoke for each item.
   */
  template <typename Func>
  void forEach(Func&& callback) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0) return;
    if (size_ < Cap) {
      for (size_t i = 0; i < size_; ++i) callback(buffer_[i]);
    } else {
      for (size_t i = 0; i < Cap; ++i) {
        callback(buffer_[(head_ + i) % Cap]);
      }
    }
  }

  // Status/Telemetry
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
  }

  constexpr size_t capacity() const { return Cap; }
  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ == 0;
  }

  T operator[](size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ < Cap) return buffer_[index];
    if (index >= Cap) return T();
    return buffer_[(head_ + index) % Cap];
  }

 private:
  mutable std::mutex mutex_;
  T buffer_[Cap];
  size_t head_ = 0;
  size_t size_ = 0;

  template <typename U>
  bool push_impl(U&& item) {
    bool overwritten = false;
    if (size_ < Cap) {
      buffer_[size_++] = std::forward<U>(item);
    } else {
      buffer_[head_] = std::forward<U>(item);
      head_ = (head_ + 1) % Cap;
      overwritten = true;
    }
    return overwritten;
  }
};

}  // namespace ebus::detail
