/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

namespace ebus {

namespace detail {

/**
 * A minimal vector-like container for uint8_t with Small Buffer Optimization
 * (SBO). Avoids heap allocations for sequences up to 64 bytes.
 */
class SmallByteVector {
 public:
  static constexpr size_t kInlineCapacity = 64;

  SmallByteVector()
      : data_(stack_buffer_), size_(0), capacity_(kInlineCapacity) {}
  ~SmallByteVector() {
    if (data_ != stack_buffer_) std::free(data_);
  }

  SmallByteVector(const SmallByteVector& other) : SmallByteVector() {
    assign(other.begin(), other.end());
  }
  SmallByteVector& operator=(const SmallByteVector& other) {
    if (this != &other) assign(other.begin(), other.end());
    return *this;
  }

  SmallByteVector(SmallByteVector&& other) noexcept : SmallByteVector() {
    moveFrom(std::move(other));
  }
  SmallByteVector& operator=(SmallByteVector&& other) noexcept {
    if (this != &other) moveFrom(std::move(other));
    return *this;
  }

  bool operator==(const SmallByteVector& other) const {
    if (size_ != other.size_) return false;
    return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
  }
  bool operator!=(const SmallByteVector& other) const {
    return !(*this == other);
  }

  void push_back(uint8_t b) {
    if (size_ >= capacity_) grow(size_ + 1);
    data_[size_++] = b;
  }
  void clear() { size_ = 0; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  uint8_t& operator[](size_t i) { return data_[i]; }
  const uint8_t& operator[](size_t i) const { return data_[i]; }

  uint8_t* begin() { return data_; }
  const uint8_t* begin() const { return data_; }
  uint8_t* end() { return data_ + size_; }
  const uint8_t* end() const { return data_ + size_; }
  const uint8_t* data() const { return data_; }

  void reserve(size_t n) {
    if (n > capacity_) grow(n);
  }
  void resize(size_t n) {
    if (n > capacity_) grow(n);
    size_ = n;
  }

  template <typename It>
  void assign(It first, It last) {
    size_t n = static_cast<size_t>(std::distance(first, last));
    if (n > capacity_) grow(n);
    std::copy(first, last, data_);
    size_ = n;
  }

  void insert(uint8_t* pos, const uint8_t* first, const uint8_t* last) {
    size_t n = static_cast<size_t>(std::distance(first, last));
    size_t offset = static_cast<size_t>(std::distance(data_, pos));
    if (size_ + n > capacity_) grow(size_ + n);
    std::move_backward(data_ + offset, data_ + size_, data_ + size_ + n);
    std::copy(first, last, data_ + offset);
    size_ += n;
  }

 private:
  void grow(size_t min_cap) {
    const size_t new_cap = std::max(min_cap, capacity_ * 2);
    if (data_ == stack_buffer_) {
      uint8_t* new_data = static_cast<uint8_t*>(std::malloc(new_cap));
      if (new_data) {
        std::memcpy(new_data, stack_buffer_, size_);
        data_ = new_data;
        capacity_ = new_cap;
      }
    } else {
      uint8_t* new_data = static_cast<uint8_t*>(std::realloc(data_, new_cap));
      if (new_data) {
        data_ = new_data;
        capacity_ = new_cap;
      }
    }
  }

  void moveFrom(SmallByteVector&& other) {
    if (data_ != stack_buffer_) std::free(data_);
    if (other.data_ == other.stack_buffer_) {
      std::memcpy(stack_buffer_, other.stack_buffer_, other.size_);
      data_ = stack_buffer_;
      capacity_ = kInlineCapacity;
    } else {
      data_ = other.data_;
      capacity_ = other.capacity_;
      other.data_ = other.stack_buffer_;
      other.capacity_ = kInlineCapacity;
    }
    size_ = other.size_;
    other.size_ = 0;
  }

  uint8_t* data_;
  size_t size_;
  size_t capacity_;
  uint8_t stack_buffer_[kInlineCapacity];
};

}  // namespace detail

/**
 * Sequence class that represents a sequence of bytes in the eBUS protocol. It
 * provides methods for constructing sequences from vectors, comparing
 * sequences, calculating CRC, and converting between extended and reduced
 * formats. The class handles byte stuffing for the special characters 0xaa and
 * 0xa9 as defined in the eBUS specification.
 *
 * (reduced) 0xaa <-> 0xa9 0x01 (extended)
 * (reduced) 0xa9 <-> 0xa9 0x00 (extended)
 */
class Sequence {
 public:
  Sequence() = default;
  Sequence(const Sequence& sequence, const size_t index, size_t len = 0);

  void assign(const std::vector<uint8_t>& vec, const bool extended = true);
  void assign(const Sequence& other, size_t index, size_t len = 0);

  void pushBack(const uint8_t byte, const bool extended = true);

  bool operator==(const Sequence& other) const;
  bool operator!=(const Sequence& other) const;
  void append(const Sequence& other);

  /**
   * Reserves capacity in the underlying buffer to avoid heap allocations.
   */
  void reserve(size_t capacity);

  const uint8_t& operator[](const size_t index) const;
  const std::vector<uint8_t> range(const size_t index, const size_t len) const;

  size_t size() const;

  void clear();
  bool isExtended() const { return extended_; }

  uint8_t crc() const;

  void extend();
  void reduce();

  std::string toString() const;
  std::vector<uint8_t> toVector() const;
  const uint8_t* data() const { return sequence_.data(); }

 private:
  detail::SmallByteVector sequence_;
  bool extended_ = false;
};

}  // namespace ebus
