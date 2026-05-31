/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <charconv>
#include <cstring>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "ebus/types.hpp"

namespace ebus {

// Forward declarations for functions used by JsonWriter but defined elsewhere
char* formatFloat(float value, int precision, char* buffer, size_t buffer_size,
                  float lower_threshold, float upper_threshold);

namespace detail {

// Declarations of functions that will be moved with JsonWriter
void writeEscapedJson(std::string_view s, const JsonChunkVisitor& visitor);
void appendHexFieldToWriter(JsonWriter& writer, ByteView data);

/**
 * @brief Helper for zero-allocation streaming JSON generation.
 * Buffers small parts and flushes to the JsonChunkVisitor to minimize calls.
 * Optimized for ESP32-C3 stack usage (256-byte buffer).
 */
class JsonWriter {
 public:
  explicit JsonWriter(JsonChunkVisitor v) : visitor_(std::move(v)) {}
  ~JsonWriter() { flush(); }

  void write(std::string_view s) {
    if (pos_ + s.size() > sizeof(buffer_)) {
      flush();
      if (s.size() > sizeof(buffer_)) {
        visitor_(s);
        return;
      }
    }
    std::memcpy(buffer_ + pos_, s.data(), s.size());
    pos_ += s.size();
  }

  void flush() {
    if (pos_ > 0) {
      visitor_(std::string_view(buffer_, pos_));
      pos_ = 0;
    }
  }

  void startObject() {
    write("{");
    first_ = true;
  }

  void endObject() {
    flush();
    write("}");
    first_ = false;
  }

  void startArray() {
    write("[");
    first_ = true;
  }

  void endArray() {
    flush();
    write("]");
    first_ = false;
  }

  void appendKey(std::string_view key) {
    if (!first_) write(",");
    write("\"");
    write(key);
    write("\":");
    first_ = true;  // The following value is the 'first' for this key
  }

  void writeEscaped(std::string_view s) {
    flush();
    ebus::detail::writeEscapedJson(s, visitor_);  // Use fully qualified name
  }

  // --- Value Writers (for arrays or raw values) ---

  void writeValue(std::string_view val) {
    if (!first_) write(",");
    write("\"");
    writeEscaped(val);
    write("\"");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<detail::has_to_json<T>::value, void> writeValue(
      const T& val) {
    if (!first_) write(",");
    flush();
    val.toJson(visitor_);
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<std::decay_t<T>, bool>, void> writeValue(
      T val) {
    if (!first_) write(",");
    write(val ? "true" : "false");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_integral_v<std::decay_t<T>> &&
                       !std::is_same_v<std::decay_t<T>, bool>,
                   void>
  writeValue(T val) {
    if (!first_) write(",");
    char buf[24];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec == std::errc{}) {
      write(std::string_view(buf, ptr - buf));
    }
    first_ = false;
  }

  void writeValueFloat(float val, int precision = 2) {
    if (!first_) write(",");
    char buf[32];
    char* end = ebus::formatFloat(
        val, precision, buf, sizeof(buf),
        ebus::detail::FormattingLimits::float_lower_threshold,
        ebus::detail::FormattingLimits::float_upper_threshold);

    write(std::string_view(buf, end - buf));
    first_ = false;
  }

  // --- Field Writers (for objects) ---

  void writeField(std::string_view key, std::string_view val) {
    appendKey(key);
    write("\"");
    writeEscaped(val);
    write("\"");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<detail::has_to_json<T>::value, void> writeField(
      std::string_view key, const T& val) {
    appendKey(key);
    flush();
    val.toJson(visitor_);
    first_ = false;
  }

  void writeField(std::string_view key, const char* val) {
    appendKey(key);
    if (val) {
      write("\"");
      writeEscaped(val);
      write("\"");
    } else {
      write("null");
    }
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<std::decay_t<T>, bool>, void> writeField(
      std::string_view key, T val) {
    appendKey(key);
    write(val ? "true" : "false");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_integral_v<std::decay_t<T>> &&
                       !std::is_same_v<std::decay_t<T>, bool>,
                   void>
  writeField(std::string_view key, T val) {
    appendKey(key);
    char buf[24];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec == std::errc{}) {
      write(std::string_view(buf, ptr - buf));
    }
    first_ = false;
  }

  void writeFieldFloat(std::string_view key, float val, int precision = 2) {
    appendKey(key);
    char buf[32];
    // Pass thresholds explicitly to match the forward declaration without
    // defaults
    char* end = ebus::formatFloat(
        val, precision, buf, sizeof(buf),
        ebus::detail::FormattingLimits::float_lower_threshold,
        ebus::detail::FormattingLimits::float_upper_threshold);

    write(std::string_view(buf, end - buf));
    first_ = false;
  }

  void writeHexField(std::string_view key, ByteView data) {
    appendKey(key);
    write("\"");
    ebus::detail::appendHexFieldToWriter(*this, data);
    write("\"");
    first_ = false;
  }

  void writeTimestampField(std::string_view key, uint64_t ms);
  void appendHexField(ByteView data);

  bool isFirst() const { return first_; }

 private:
  JsonChunkVisitor visitor_;
  char buffer_[256];
  size_t pos_ = 0;
  bool first_ = true;
};

}  // namespace detail
}  // namespace ebus
