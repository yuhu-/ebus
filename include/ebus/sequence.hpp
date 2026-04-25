/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include "ebus/defaults.hpp"
#include "ebus/protocol_math.hpp"
#include "ebus/types.hpp"

namespace ebus {

namespace detail {

/**
 * A minimal vector-like container for uint8_t with Small Buffer Optimization
 * (SBO). Avoids heap allocations for sequences up to 64 bytes.
 */
template <typename T = uint8_t,
          size_t kInlineCapacity = defaults::Sequence::default_capacity>
class SmallByteVector {
 public:
  static_assert(std::is_trivially_copyable_v<T>,
                "SmallByteVector requires trivially copyable type");

  SmallByteVector()
      : data_(stack_buffer_),
        size_(0),
        capacity_(kInlineCapacity),
        stack_buffer_{} {}
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

  bool operator!=(const SmallByteVector<T, kInlineCapacity>& other) const {
    return !(*this == other);
  }

  void push_back(T b) {
    if (size_ >= capacity_) grow(size_ + 1);
    data_[size_++] = b;
  }
  void clear() { size_ = 0; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T& operator[](size_t i) { return data_[i]; }
  const T& operator[](size_t i) const { return data_[i]; }

  T* begin() { return data_; }
  const T* begin() const { return data_; }
  T* end() { return data_ + size_; }
  const T* end() const { return data_ + size_; }
  const T* data() const { return data_; }

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

  void insert(T* pos, const T* first, const T* last) {
    size_t n = static_cast<size_t>(std::distance(first, last));
    size_t offset = static_cast<size_t>(std::distance(data_, pos));
    if (size_ + n > capacity_) grow(size_ + n);
    std::move_backward(data_ + offset, data_ + size_, data_ + size_ + n);
    std::copy(first, last, data_ + offset);
    size_ += n;
  }

 private:
  T* data_;
  size_t size_;
  size_t capacity_;
  T stack_buffer_[kInlineCapacity];

  void grow(size_t min_cap) {
    const size_t new_cap = std::max(min_cap, capacity_ * 2);
    if (data_ == stack_buffer_) {
      T* new_data = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
      if (new_data) {
        std::memcpy(new_data, stack_buffer_, size_ * sizeof(T));
        data_ = new_data;
        capacity_ = new_cap;
      }
    } else {
      T* new_data = static_cast<T*>(std::realloc(data_, new_cap * sizeof(T)));
      if (new_data) {
        data_ = new_data;
        capacity_ = new_cap;
      }
    }
  }

  void moveFrom(SmallByteVector<T, kInlineCapacity>&& other) {
    if (data_ != stack_buffer_) std::free(data_);
    if (other.data_ == other.stack_buffer_) {
      std::memcpy(stack_buffer_, other.stack_buffer_, other.size_ * sizeof(T));
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
template <size_t kInlineCapacity = defaults::Sequence::default_capacity>
class SequenceImpl {
 public:
  static_assert(kInlineCapacity >= defaults::Sequence::max_telegram_bytes,
                "Sequence capacity too small");

  SequenceImpl() = default;

  /**
   * Slicing constructor.
   */
  SequenceImpl(const SequenceImpl& sequence, const size_t index,
               size_t len = 0) {
    if (index >= sequence.size()) {
      extended_ = sequence.isExtended();
      return;
    }
    if (len == 0 || index + len > sequence.size())
      len = sequence.size() - index;
    sequence_.reserve(len);
    sequence_.resize(len);
    std::copy(sequence.begin() + index, sequence.begin() + index + len,
              sequence_.begin());
    extended_ = sequence.isExtended();
  }

  SequenceImpl(std::initializer_list<uint8_t> list) {
    sequence_.assign(list.begin(), list.end());
  }

  /**
   * Compile-time size-checked constructor for raw arrays.
   */
  template <size_t N>
  explicit SequenceImpl(const uint8_t (&arr)[N]) {
    static_assert(N <= kInlineCapacity,
                  "Initial data exceeds stack buffer capacity.");
    sequence_.assign(arr, arr + N);
  }

  void assignSlice(const SequenceImpl& other, size_t index, size_t len = 0) {
    if (index >= other.size()) {
      clear();
      extended_ = other.isExtended();
      return;
    }
    if (len == 0 || index + len > other.size()) len = other.size() - index;
    sequence_.assign(other.begin() + index, other.begin() + index + len);
    extended_ = other.isExtended();
  }

  /**
   * Assigns data from a ByteView.
   */
  void assign(ByteView view, bool extended = false) {
    sequence_.assign(view.begin(), view.end());
    extended_ = extended;
  }

  /**
   * Assigns data from an initializer list.
   */
  void assign(std::initializer_list<uint8_t> list, bool extended = false) {
    sequence_.assign(list.begin(), list.end());
    extended_ = extended;
  }

  /**
   * Compares two sequences based on their internal data and extension state.
   */
  bool operator==(const SequenceImpl& other) const {
    return sequence_ == other.sequence_ && extended_ == other.extended_;
  }

  bool operator!=(const SequenceImpl& other) const { return !(*this == other); }

  /**
   * Compares two sequences semantically. Returns true if they represent the
   * same protocol data, even if one is extended (wire format) and the other
   * is reduced (logical format).
   */
  template <size_t OtherCap>
  bool logicallyEquals(const SequenceImpl<OtherCap>& other) const {
    if (extended_ == other.isExtended()) {
      return sequence_ == other.sequence_;
    }
    // Normalize both to reduced format for comparison
    SequenceImpl<kInlineCapacity> a = *this;
    SequenceImpl<OtherCap> b = other;
    a.reduce();
    b.reduce();
    return a.sequence_ == b.sequence_;
  }

  void pushBack(const uint8_t byte, const bool extended = true) {
    sequence_.push_back(byte);
    extended_ = extended;
  }

  uint8_t operator[](size_t index) const { return sequence_[index]; }

  /**
   * Appends data while normalizing to the target extension state.
   */
  template <size_t OtherCap>
  void append(const SequenceImpl<OtherCap>& other) {
    SequenceImpl temp = other;
    if (extended_)
      temp.extend();
    else
      temp.reduce();
    sequence_.insert(sequence_.end(), temp.begin(), temp.end());
  }

  /**
   * Reserves capacity in the underlying buffer to avoid heap allocations.
   */
  void reserve(size_t capacity) { sequence_.reserve(capacity); }

  void clear() {
    sequence_.clear();
    extended_ = false;
  }
  size_t size() const { return sequence_.size(); }
  bool empty() const { return size() == 0; }

  const uint8_t* data() const { return sequence_.data(); }
  const uint8_t* begin() const { return sequence_.begin(); }
  const uint8_t* end() const { return sequence_.end(); }
  uint8_t* begin() { return sequence_.begin(); }
  uint8_t* end() { return sequence_.end(); }

  bool isExtended() const { return extended_; }

  /**
   * Returns true if the sequence contains bytes that require eBUS stuffing
   * (0xAA or 0xA9).
   */
  bool needsExtension() const noexcept {
    return std::any_of(sequence_.begin(), sequence_.end(),
                       [](uint8_t b) { return Symbols::needsEscape(b); });
  }

  /**
   * Returns true if the sequence is extended and contains bytes that require
   * eBUS unstuffing (A9 00 or A9 01).
   */
  bool needsReduction() const noexcept {
    if (!extended_) return false;
    return std::any_of(sequence_.begin(), sequence_.end(),
                       [](uint8_t b) { return b == Symbols::ext; });
  }

  void extend() {
    if (extended_) return;

    const size_t extra =
        std::count_if(sequence_.begin(), sequence_.end(),
                      [](uint8_t b) { return Symbols::needsEscape(b); });

    if (extra == 0) {
      extended_ = true;
      return;
    }

    size_t old_size = sequence_.size();
    size_t new_size = old_size + extra;
    sequence_.resize(new_size);
    size_t write_idx = new_size - 1;
    for (int i = static_cast<int>(old_size) - 1; i >= 0; --i) {
      uint8_t b = sequence_[i];
      if (Symbols::needsEscape(b)) {
        uint8_t escaped[2];
        Symbols::escape(b, escaped);
        sequence_[write_idx--] = escaped[1];
        sequence_[write_idx--] = escaped[0];
      } else {
        sequence_[write_idx--] = b;
      }
    }
    extended_ = true;
  }

  void reduce() {
    if (!extended_) return;
    if (!needsReduction()) {
      extended_ = false;
      return;
    }

    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < sequence_.size(); ++read_idx) {
      uint8_t unescaped;
      if (read_idx + 1 < sequence_.size() &&
          Symbols::unescape(sequence_[read_idx], sequence_[read_idx + 1],
                            unescaped)) {
        sequence_[write_idx++] = unescaped;
        ++read_idx;
      } else {
        sequence_[write_idx++] = sequence_[read_idx];
      }
    }
    sequence_.resize(write_idx);
    extended_ = false;
  }

  uint8_t crc() const {
    uint8_t current_crc = Symbols::zero;
    for (uint8_t byte : sequence_) {
      if (!extended_) {
        if (Symbols::needsEscape(byte)) {
          uint8_t escaped[2];
          Symbols::escape(byte, escaped);
          current_crc = calcCRC(escaped[0], current_crc);
          current_crc = calcCRC(escaped[1], current_crc);
        } else {
          current_crc = calcCRC(byte, current_crc);
        }
      } else {
        current_crc = calcCRC(byte, current_crc);
      }
    }
    return current_crc;
  }

  ByteView range(size_t index, size_t len) const {
    if (index >= size()) return {};
    size_t count = std::min(len, size() - index);
    return ByteView(data() + index, count);
  }

  std::string toString() const {
    static constexpr char hex_chars[] = "0123456789abcdef";
    std::string res;
    res.reserve(size() * 2);
    for (auto b : sequence_) {
      res.push_back(hex_chars[b >> 4]);
      res.push_back(hex_chars[b & 0xf]);
    }
    return res;
  }

  std::vector<uint8_t> toVector() const {
    return std::vector<uint8_t>(sequence_.begin(), sequence_.end());
  }

  /**
   * Implicit conversion to ByteView for zero-copy interoperability.
   */
  operator ByteView() const { return ByteView(data(), size()); }

 private:
  detail::SmallByteVector<uint8_t, kInlineCapacity> sequence_;
  bool extended_ = false;
};

/**
 * Default eBUS sequence with 64-byte SBO buffer.
 */
using Sequence = SequenceImpl<defaults::Sequence::default_capacity>;

/**
 * Factory function to create a sequence from a raw ByteView.
 */
template <size_t N = defaults::Sequence::default_capacity>
SequenceImpl<N> makeSequence(ByteView data, bool extended = false) {
  SequenceImpl<N> seq;
  seq.assign(data, extended);
  return seq;
}

}  // namespace ebus
