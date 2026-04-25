/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <vector>

namespace ebus {
namespace detail {

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
    
    // Pushes an item into the buffer. Returns true if an old element was overwritten.
    bool push_back(T&& item) {
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
        buffer_.clear();
        head_ = 0;
    }
    
    // Resizes the buffer to a new capacity, clearing existing elements.
    void set_capacity(size_t capacity) {
        buffer_.clear();
        buffer_.shrink_to_fit();
        buffer_.reserve(capacity);
        head_ = 0;
    }

    size_t size() const { return buffer_.size(); }
    size_t capacity() const { return buffer_.capacity(); }
    bool empty() const { return buffer_.empty(); }

    T const& operator[](size_t index) const {
        if (buffer_.size() < buffer_.capacity()) return buffer_[index];
        return buffer_[(head_ + index) % buffer_.capacity()];
    }

private:
    std::vector<T> buffer_;
    size_t head_ = 0;
};

} // namespace detail
} // namespace ebus
