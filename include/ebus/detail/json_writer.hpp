/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <charconv>
#include <cstring>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "ebus/detail/protocol_limits.hpp"
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
  /**
   * @brief RAII helper to ensure JSON containers (objects/arrays) are closed.
   */
  class Scope {
   public:
    enum Type { Object, Array };

    Scope(JsonWriter& writer, Type type) : writer_(writer), type_(type) {
      if (type_ == Object) {
        writer_.startObject();
      } else {
        writer_.startArray();
      }
    }

    ~Scope() {
      if (type_ == Object) {
        writer_.endObject();
      } else {
        writer_.endArray();
      }
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

   private:
    JsonWriter& writer_;
    Type type_;
  };

  explicit JsonWriter(JsonChunkVisitor v, bool pretty = false)
      : visitor_(std::move(v)), buffer_{}, pretty_(pretty) {}
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
    beforeValue();
    write("{");
    if (pretty_) indent_level_++;
    first_ = true;
  }

  void endObject() {
    if (pretty_) {
      indent_level_--;
      writeIndent();
    }
    write("}");
    flush();
    first_ = false;
    after_key_ = false;
  }

  void startArray() {
    beforeValue();
    write("[");
    if (pretty_) indent_level_++;
    first_ = true;
  }

  void endArray() {
    if (pretty_) {
      indent_level_--;
      writeIndent();
    }
    write("]");
    flush();
    first_ = false;
    after_key_ = false;
  }

  void appendKey(std::string_view key) {
    if (!first_) write(",");
    if (pretty_) writeIndent();
    write("\"");
    write(key);
    write("\":");
    if (pretty_) write(" ");
    first_ = true;  // The following value is the 'first' for this key
    after_key_ = true;
  }

  void writeEscaped(std::string_view s) {
    flush();
    ebus::detail::writeEscapedJson(s, visitor_);  // Use fully qualified name
  }

  // --- Value Writers (for arrays or raw values) ---

  void writeValue(std::string_view val) {
    beforeValue();
    write("\"");
    writeEscaped(val);
    write("\"");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<detail::has_to_json<T>::value, void> writeValue(
      const T& val) {
    beforeValue();
    first_ = true;  // Inform child toJson that comma is already handled
    flush();
    val.toJson(*this);
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<std::decay_t<T>, bool>, void> writeValue(
      T val) {
    beforeValue();
    write(val ? "true" : "false");
    first_ = false;
  }

  template <typename T>
  std::enable_if_t<std::is_integral_v<std::decay_t<T>> &&
                       !std::is_same_v<std::decay_t<T>, bool>,
                   void>
  writeValue(T val) {
    beforeValue();
    char buf[24];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec == std::errc{}) {
      write(std::string_view(buf, ptr - buf));
    }
    first_ = false;
  }

  void writeValueFloat(float val, int precision = 2) {
    beforeValue();
    char buf[32];
    const char* end = ebus::formatFloat(
        val, precision, buf, sizeof(buf),
        ebus::detail::FormattingLimits::float_lower_threshold,
        ebus::detail::FormattingLimits::float_upper_threshold);

    write(std::string_view(buf, end - buf));
    first_ = false;
  }

  void writeHexValue(ByteView data) {
    beforeValue();
    write("\"");
    ebus::detail::appendHexFieldToWriter(*this, data);
    write("\"");
    first_ = false;
  }

  // --- Field Writers (for objects) ---

  void writeRaw(std::string_view s) {
    if (pretty_ && !after_key_ && indent_level_ > 0) writeIndent();
    after_key_ = false;
    write(s);
    first_ = false;
  }

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
    first_ = true;  // Inform child toJson that comma is already handled
    flush();
    val.toJson(*this);
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
    const char* end = ebus::formatFloat(
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

  bool isFirst() const { return first_; }

 private:
  JsonChunkVisitor visitor_;
  char buffer_[256];  // Optimized for ESP32-C3 stack usage
  size_t pos_ = 0;
  bool first_ = true;
  bool pretty_ = false;
  int indent_level_ = 0;
  bool after_key_ = false;

  void beforeValue() {
    if (!first_) write(",");
    if (pretty_ && !after_key_ && indent_level_ > 0) writeIndent();
    after_key_ = false;
  }

  void writeIndent() {
    if (!pretty_) return;
    write("\n");
    for (int i = 0; i < indent_level_; ++i) write("  ");
  }
};

}  // namespace detail
}  // namespace ebus
