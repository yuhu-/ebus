/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string_view>

#include "ebus/utils.hpp"

namespace ebus::detail {

/**
 * @brief Minimal zero-allocation Pull-Parser for JSON.
 * Operates directly on a string_view and provides tokens.
 */
class JsonReader {
 public:
  enum class Token {
    End,
    ObjectStart,
    ObjectEnd,
    ArrayStart,
    ArrayEnd,
    Key,
    String,
    Number,
    Boolean,
    Null,
    Error
  };

  explicit JsonReader(std::string_view json) : json_(json) { skipWhitespace(); }

  /**
   * @brief Resets the reader to the beginning of the JSON string.
   */
  void reset();

  /**
   * @brief Advances to the next token in the stream.
   */
  Token next();

  /**
   * @brief Returns the value of the current token (string, key, or number).
   */
  std::string_view value() const { return value_; }

  /**
   * @brief Extracts the next value as a raw JSON fragment.
   * Consumes the value and advances the reader.
   */
  std::string_view rawValue();

  /**
   * @brief Helper to find a specific key within the current object scope.
   * Note: This consumes tokens until the key is found or the object ends.
   */
  bool findKey(std::string_view target_key);

  /**
   * @brief Searches for an element in an array that satisfies a predicate.
   * The reader must be positioned at an ArrayStart token.
   * @param pred A callable: `bool(JsonReader& reader)`.
   * @return true if an element was found. The reader will be positioned
   * inside that element.
   */
  template <typename Predicate>
  bool find(Predicate pred) {
    while (true) {
      size_t element_start = pos_;
      Token t = next();
      if (t == Token::ArrayEnd || t == Token::End || t == Token::Error) {
        return false;
      }

      size_t after_token = pos_;
      pos_ = element_start;  // Rewind to let predicate see the value
      if (pred(*this)) return true;
      pos_ = after_token;  // Ensure we are past the start of the value
      skipValue();         // Skip the rest of the element
    }
  }

  /**
   * @brief Iterates over fields in an object.
   * The reader must be positioned at an ObjectStart token.
   * @param func A callable: `bool(std::string_view key, JsonReader& reader)`.
   * Should return true if the field was handled (and the value
   * consumed/advanced), false otherwise to let the reader skip the value
   * automatically.
   */
  template <typename Func>
  void forEachField(Func func) {
    while (true) {
      Token t = next();
      if (t == Token::ObjectEnd || t == Token::End || t == Token::Error) {
        break;
      }
      if (t == Token::Key) {
        if (!func(value(), *this)) skipValue();
      }
    }
  }

  /**
   * @brief Skips the current value in the stream.
   * If the current token is a container start, it skips until the end
   * of that container.
   */
  void skipValue();

  /**
   * @brief Skips the current value. If it's a composite (Object/Array), it
   * handles nesting. Assumes next() has just returned the start token.
   */
  void skipComposite(Token start_token);

  /**
   * @brief Performs a strict structural validation of the JSON string.
   * Checks for correct comma placement, colon usage, and balanced containers.
   * @return true if the JSON is strictly valid.
   */
  static bool validate(std::string_view json);

  /**
   * @brief Navigates to a specific JSON path (e.g., "bus.window_us").
   * @param path Dot-separated path.
   * @return The token type of the value at that path, or Token::Error if not
   * found.
   * @note This method resets the reader's internal position and re-parses from
   * the beginning.
   */
  Token get(std::string_view path);

  /**
   * @brief Convenience: Reads the current value as a number.
   */
  template <typename T>
  T asNum() const {
    return ebus::toNum<T>(value_);
  }

  /**
   * @brief Reads the current value as a number with strict validation.
   */
  template <typename T>
  std::optional<T> asNumStrict() const {
    return ebus::toNumStrict<T>(value_);
  }

  bool asBool() const { return value_ == "true"; }

 private:
  std::string_view json_;
  std::string_view value_;
  size_t pos_ = 0;

  void skipWhitespace();
  void skipToClosing();

  std::string_view readString();

  static std::optional<size_t> parseIndex(std::string_view s);
};

}  // namespace ebus::detail
