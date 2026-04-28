/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <mutex>
#include <vector>

namespace ebus::detail {

/**
 * A simple circular buffer utility using a pre-allocated vector.
 * Provides chronological access and zero-allocation updates once at capacity.
 */
template <typename T>
class CircularBuffer {
 public:
  explicit CircularBuffer(size_t capacity = 0) : head_(0) {
    if (capacity > 0) buffer_.reserve(capacity);
  }

  // Pushes an item into the buffer. Returns true if an old element was
  // overwritten.
  bool push_back(T&& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t cap = buffer_.capacity();
    if (cap == 0) return false;
    bool overwritten = false;
    if (buffer_.size() < cap) {
      buffer_.push_back(std::move(item));
    } else {
      buffer_[head_] = std::move(item);
      head_ = (head_ + 1) % cap;
      overwritten = true;
    }
    return overwritten;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    head_ = 0;
  }

  // Resizes the buffer to a new capacity, clearing existing elements.
  void set_capacity(size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    buffer_.shrink_to_fit();
    buffer_.reserve(capacity);
    head_ = 0;
  }

  // Returns a copy of the buffer's contents in chronological order.
  std::vector<T> snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.empty()) return {};

    std::vector<T> result;
    result.reserve(buffer_.size());

    if (buffer_.size() < buffer_.capacity()) {
      result.assign(buffer_.begin(), buffer_.end());
    } else {
      // Contents from head to end (older) then start to head (newer)
      result.insert(result.end(), buffer_.begin() + head_, buffer_.end());
      result.insert(result.end(), buffer_.begin(), buffer_.begin() + head_);
    }
    return result;
  }

  size_t size() const { return buffer_.size(); }
  size_t capacity() const { return buffer_.capacity(); }
  bool empty() const { return buffer_.empty(); }

  T operator[](size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.size() < buffer_.capacity()) return buffer_[index];
    return buffer_[(head_ + index) % buffer_.capacity()];
  }

 private:
  mutable std::mutex mutex_;
  std::vector<T> buffer_;
  size_t head_ = 0;
};

}  // namespace ebus::detail
