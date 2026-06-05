/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace ebus::detail {

/**
 * @brief A fixed-capacity vector that avoids heap allocations.
 *
 * Elements are stored inline. The capacity is fixed at compile time,
 * but the size is dynamic up to that capacity. Elements are only
 * constructed when added and are destroyed when removed.
 *
 * Optimized for the Orchestration Path on resource-constrained targets.
 */
template <typename T, std::size_t Cap>
class StaticVector {
 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  // Lifecycle
  StaticVector() = default;
  ~StaticVector() { clear(); }

  // Special Members & Operators
  StaticVector(const StaticVector& other) {
    for (const auto& item : other) {
      push_back(item);
    }
  }

  StaticVector& operator=(const StaticVector& other) {
    if (this != &other) {
      clear();
      for (const auto& item : other) push_back(item);
    }
    return *this;
  }

  StaticVector& operator=(StaticVector&& other) noexcept {
    if (this != &other) {
      clear();
      for (std::size_t i = 0; i < other.size_; ++i) {
        emplace_back(std::move(other[i]));
      }
      other.clear();
    }
    return *this;
  }

  StaticVector(StaticVector&& other) noexcept {
    for (std::size_t i = 0; i < other.size_; ++i) {
      emplace_back(std::move(other[i]));
    }
    other.clear();
  }

  // Working Methods
  bool push_back(const T& value) {
    if (size_ >= Cap) return false;
    new (ptr(size_++)) T(value);
    return true;
  }

  bool push_back(T&& value) {
    if (size_ >= Cap) return false;
    new (ptr(size_++)) T(std::move(value));
    return true;
  }

  template <typename... Args>
  T* emplace_back(Args&&... args) {
    if (size_ >= Cap) return nullptr;
    return new (ptr(size_++)) T(std::forward<Args>(args)...);
  }

  void pop_back() {
    if (size_ > 0) {
      ptr(--size_)->~T();
    }
  }

  iterator erase(iterator pos) {
    if (pos < begin() || pos >= end()) return end();
    std::size_t idx = std::distance(begin(), pos);
    std::move(pos + 1, end(), pos);
    ptr(--size_)->~T();
    return begin() + idx;
  }

  iterator erase(iterator first, iterator last) {
    if (first == last) return first;
    if (first < begin() || last > end() || first > last) return end();
    std::size_t idx = std::distance(begin(), first);
    std::size_t n = std::distance(first, last);
    std::move(last, end(), first);
    for (std::size_t i = 0; i < n; ++i) {
      ptr(--size_)->~T();
    }
    return begin() + idx;
  }

  void clear() {
    while (size_ > 0) {
      pop_back();
    }
  }

  T& operator[](std::size_t i) { return *ptr(i); }
  const T& operator[](std::size_t i) const { return *ptr(i); }

  T& front() { return *ptr(0); }
  const T& front() const { return *ptr(0); }
  T& back() { return *ptr(size_ - 1); }
  const T& back() const { return *ptr(size_ - 1); }

  iterator begin() noexcept { return ptr(0); }
  const_iterator begin() const noexcept { return ptr(0); }
  iterator end() noexcept { return ptr(size_); }
  const_iterator end() const noexcept { return ptr(size_); }

  // Status/Telemetry
  std::size_t size() const noexcept { return size_; }
  constexpr std::size_t capacity() const noexcept { return Cap; }
  bool empty() const noexcept { return size_ == 0; }
  bool full() const noexcept { return size_ == Cap; }

 private:
  T* ptr(std::size_t i) { return reinterpret_cast<T*>(&storage_[i]); }

  const T* ptr(std::size_t i) const {
    return reinterpret_cast<const T*>(&storage_[i]);
  }

  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_[Cap];
  std::size_t size_ = 0;
};

}  // namespace ebus::detail
