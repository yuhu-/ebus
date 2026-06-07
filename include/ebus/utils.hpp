/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ebus/detail/json_writer.hpp"
#include "ebus/detail/protocol_limits.hpp"
#include "ebus/protocol_math.hpp"
#include "ebus/types.hpp"  // For JsonChunkVisitor, ByteView

namespace ebus {

/**
 * Lock-free update of a maximum value stored in an atomic variable.
 * Uses a compare-and-swap loop to ensure thread safety without mutexes.
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
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
template <typename T,
          typename = std::enable_if_t<detail::has_to_json<T>::value>>
std::string toJson(const T& obj, const size_t reserve, bool pretty = false) {
  std::string json;
  json.reserve(reserve);
  detail::JsonWriter writer([&json](std::string_view s) { json.append(s); },
                            pretty);
  obj.toJson(writer);
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
 * @brief Zero-allocation string-to-number converter.
 * Lenient: stops at the first non-numeric character. Returns 0 on error.
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
inline T toNum(std::string_view s) {
  if (s.empty() || s == "null") return 0;
  T val = 0;
  if constexpr (std::is_floating_point_v<T>) {
    // Fallback for environments (like ESP32-C3) missing floating-point
    // std::from_chars.
    char buf[64];
    const size_t len = (s.size() < sizeof(buf)) ? s.size() : sizeof(buf) - 1;
    std::memcpy(buf, s.data(), len);
    buf[len] = '\0';
    char* end;
    if constexpr (std::is_same_v<T, float>)
      val = std::strtof(buf, &end);
    else
      val = static_cast<T>(std::strtod(buf, &end));
  } else {
    std::string_view sv = s;
    int base = (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X'))
                   ? 16
                   : 10;
    if (base == 16) {
      sv.remove_prefix(2);
    }
    std::from_chars(sv.data(), sv.data() + sv.size(), val, base);
  }
  return val;
}

/**
 * @brief Strict string-to-number converter.
 * @return std::optional containing the value if the entire string was numeric,
 *         otherwise nullopt.
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
inline std::optional<T> toNumStrict(std::string_view s) {
  if (s.empty() || s == "null") return std::nullopt;
  T val = 0;

  if constexpr (!std::is_floating_point_v<T>) {
    std::string_view sv = s;
    int base = 10;
    if (sv.size() > 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
      sv.remove_prefix(2);
      base = 16;
    }
    auto [ptr, ec] =
        std::from_chars(sv.data(), sv.data() + sv.size(), val, base);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return val;
  }
  // For floats, fall back to lenient toNum or implement similar check if needed
  return toNum<T>(s);
}

/**
 * @brief Returns the current system wall-clock time in milliseconds since
 * epoch.
 */
inline uint64_t getWallTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
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
template <typename T,
          typename = std::enable_if_t<detail::is_byte_range<T>::value>>
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
template <typename T,
          typename = std::enable_if_t<detail::is_byte_range<T>::value>>
inline ByteView range(const T& container, size_t index, size_t len) {
  if (index >= container.size()) return {};
  size_t count = std::min(len, container.size() - index);
  return ByteView(container.data() + index, count);
}

/**
 * Checks if a container contains a search pattern.
 */
template <typename T, typename U,
          typename = std::enable_if_t<detail::is_byte_range<T>::value &&
                                      detail::is_byte_range<U>::value>>
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

/**
 * Checks if a search pattern matches a container at a specific index.
 */
template <typename T, typename U,
          typename = std::enable_if_t<detail::is_byte_indexable<T>::value &&
                                      detail::is_byte_range<U>::value>>
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