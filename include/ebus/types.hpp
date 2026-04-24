/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

#include "ebus/enums.hpp"

namespace ebus {

/**
 * Represents a single event on the bus, including the byte value, whether it
 * was associated with a bus request or start bit, and the timestamp of when it
 * was captured.
 */
struct BusEvent {
  uint8_t byte;
  bool bus_request{false};
  bool start_bit{false};
  std::chrono::steady_clock::time_point timestamp;
};

/**
 * Snapshot of the eBUS FSM state at the moment a byte was processed.
 */
struct BusEventContext {
  uint8_t byte;
  RequestState state;
  RequestResult result;
  uint8_t lock_counter;
  std::chrono::steady_clock::time_point timestamp;
};

/**
 * A lightweight, non-owning view of a byte sequence.
 * Similar to std::string_view but for uint8_t.
 */
struct ByteView {
  constexpr ByteView() = default;
  constexpr ByteView(const uint8_t* data, size_t size)
      : data_(data), size_(size) {}

  // Implicit conversion from std::vector is intentional to allow transparent
  // usage of owning containers in functions accepting views.
  ByteView(const std::vector<uint8_t>& v) : data_(v.data()), size_(v.size()) {}

  constexpr const uint8_t* data() const noexcept { return data_; }
  constexpr size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  constexpr const uint8_t* begin() const noexcept { return data_; }
  constexpr const uint8_t* end() const noexcept { return data_ + size_; }

  constexpr uint8_t operator[](size_t i) const { return data_[i]; }

  bool operator==(ByteView other) const {
    if (this == &other) return true;
    if (size_ != other.size_) return false;
    return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
  }
  bool operator!=(ByteView other) const { return !(*this == other); }

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
};

// Ensure definitions.hpp remains lean and free of heavy template logic
static_assert(std::is_standard_layout_v<ByteView>,
              "ByteView must maintain standard layout for ABI compatibility.");
static_assert(std::is_trivially_copyable_v<ByteView>,
              "ByteView must be trivially copyable to remain heap-free in the "
              "hot path.");

}  // namespace ebus
