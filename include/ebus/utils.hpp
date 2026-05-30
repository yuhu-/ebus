/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ebus/detail/protocol_limits.hpp"
#include "ebus/protocol_math.hpp"
#include "ebus/types.hpp"  // For JsonChunkVisitor, ByteView

namespace ebus {

/**
 * Lock-free update of a maximum value stored in an atomic variable.
 * Uses a compare-and-swap loop to ensure thread safety without mutexes.
 */
template <typename T>
inline void updateMaxAtomic(std::atomic<T>& atomic_max, T value) {
  T current_max = atomic_max.load(std::memory_order_relaxed);
  while (value > current_max &&
         !atomic_max.compare_exchange_weak(current_max, value,
                                           std::memory_order_relaxed));
}

/**
 * @brief Global helper to create a JSON string for any object supporting
 * the toJson(const JsonChunkVisitor&) pattern.
 */
template <typename T>
std::string toJson(const T& obj, const size_t reserve) {
  std::string json;
  json.reserve(reserve);
  obj.toJson([&json](std::string_view s) { json.append(s); });
  return json;
}

/**
 * Escapes a string for use in a JSON value.
 */
std::string escapeJson(std::string_view s);

/**
 * @brief Writes an escaped version of the string to the provided visitor.
 * Prevents temporary string allocations.
 */
void writeEscapedJson(std::string_view s, const JsonChunkVisitor& visitor);

/**
 * Appends an escaped version of the string to the provided buffer.
 * Prevents temporary string allocations during JSON serialization.
 */
void appendEscapedJson(std::string& out, std::string_view s);

/**
 * Appends data as a hex string to the provided buffer.
 */
void appendHex(std::string& out, ByteView data);

/**
 * Forward declaration for Sequence to break header dependency.
 */
template <std::size_t kInlineCapacity>
class SequenceImpl;
using Sequence = SequenceImpl<detail::SequenceLimits::default_capacity>;

// --- JSON Parsing Helpers ---

/**
 * Finds a key safely by ensuring it is wrapped in quotes and followed by a
 * colon.
 */
size_t findKey(std::string_view json, std::string_view key);
std::string_view extract(std::string_view json, std::string_view key);
std::string_view extractSub(std::string_view json, std::string_view key);

/**
 * Zero-allocation string-to-number converter.
 */
template <typename T>
T toNum(std::string_view s) {
  if (s.empty() || s == "null") return 0;
  T val = 0;
  std::from_chars(s.data(), s.data() + s.size(), val);
  return val;
}

/**
 * @brief Fast ISO8601 formatter for embedded targets.
 * Converts milliseconds since epoch to "YYYY-MM-DDTHH:MM:SS.mmmZ".
 * Zero-allocation and avoids heavy printf/strftime.
 */
void formatIso8601Fast(uint64_t ms_since_epoch, char* out);

/**
 * Formats a float to a buffer with scientific/fixed switching.
 */
char* formatFloat(float value, int precision, char* buffer, size_t buffer_size,
                  float lower_threshold, float upper_threshold);
std::string toString(uint8_t byte);

std::string byteToChar(ByteView data);

std::string byteToHex(ByteView data);

/**
 * Converts any byte container to a hex string.
 */
template <typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>>>
std::string toString(const T& container) {
  if (std::empty(container)) return {};
  return byteToHex(ByteView(std::data(container), std::size(container)));
}
std::vector<uint8_t> toVector(const std::string& str);

/**
 * Converts a non-owning ByteView to an owning std::vector.
 */
inline std::vector<uint8_t> toVector(ByteView data) {
  return {data.begin(), data.end()};
}

/**
 * Helper to frame a master telegram with CRC from a ByteView.
 * @param source The source address.
 * @param payload The master payload bytes (QQ - DBx).
 * @return The framed master telegram (QQ - DBx + CRC).
 */
Sequence frameMaster(uint8_t source, ByteView payload);

std::string frameMasterHex(uint8_t source, ByteView payload);

std::string frameMasterHex(uint8_t source, const std::string& payload);

/**
 * Helper to frame a slave response with CRC from a ByteView.
 * Does NOT include the leading ACK or NAK byte.
 * @param payload The slave payload bytes (NN + DBx).
 * @return The framed slave response (NN + DBx + CRC).
 */
Sequence frameSlave(ByteView payload);

std::string frameSlaveHex(ByteView payload);

std::string frameSlaveHex(const std::string& payload);

/**
 * Rounds a floating point value to a specific number of decimal places.
 */
float roundDigits(float value, uint8_t digits) noexcept;

// --- Vector Helpers ---

/**
 * Returns a non-owning ByteView of a range within a container.
 */
template <typename T>
ByteView range(const T& container, size_t index, size_t len) {
  if (index >= container.size()) return {};
  size_t count = std::min(len, container.size() - index);
  return ByteView(container.data() + index, count);
}

template <typename T, typename U>
bool contains(const T& container, const U& search) {
  if (std::empty(search) || std::empty(container) ||
      std::size(search) > std::size(container))
    return false;
  return std::search(container.begin(), container.end(), search.begin(),
                     search.end()) != container.end();
}

/**
 * Overload for contains to support brace-enclosed initializer lists.
 */
template <typename T>
bool contains(const T& container, std::initializer_list<uint8_t> search) {
  return contains<T, std::initializer_list<uint8_t>>(container, search);
}

template <typename T, typename U>
inline bool matches(const T& container, const U& search, size_t index = 0) {
  const size_t search_size = std::size(search);
  const size_t cont_size = std::size(container);
  if (search_size == 0) return true;  // Empty search always matches
  if (index >= cont_size || index + search_size > cont_size)
    return false;  // Out of bounds
  auto it = std::begin(search);
  for (size_t i = 0; i < search_size; ++i) {
    if (container[index + i] != *it) return false;
    ++it;
  }
  return true;
}

/**
 * Overload for matches to support brace-enclosed initializer lists.
 */
template <typename T>
bool matches(const T& container, std::initializer_list<uint8_t> search,
             size_t index = 0) {
  return matches<T, std::initializer_list<uint8_t>>(container, search, index);
}

}  // namespace ebus