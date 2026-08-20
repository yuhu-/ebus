/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cctype>
#include <charconv>
#include <cstring>
#include <ebus/detail/json_reader.hpp>

namespace ebus::detail {

void JsonReader::reset() {
  pos_ = 0;
  buffer_pos_ = 0;
  value_ = {};
  need_more_data_ = false;
  rawvalue_start_ = 0;
  rawvalue_in_progress_ = false;
  skip_to_closing_in_string_ = false;
  skip_to_closing_depth_ = 1;
  if (streaming_mode_) {
    if (!fixed_buffer_.data()) {
      buffer_.clear();
    }
    json_ = {};
  }
  skipWhitespace();
}

JsonReader::Token JsonReader::next() {
  skipWhitespace();
  if (pos_ >= json_.size()) {
    if (ended_) return Token::end;
    need_more_data_ = true;
    return Token::need_more_data;
  }

  need_more_data_ = false;

  char c = json_[pos_];

  // Skip separators between tokens
  if (c == ':' || c == ',') {
    pos_++;
    skipWhitespace();
    if (pos_ >= json_.size()) {
      if (ended_) return Token::error;
      need_more_data_ = true;
      return Token::need_more_data;
    }
    c = json_[pos_];
  }

  switch (c) {
    case '{':
      pos_++;
      return Token::object_start;
    case '}':
      pos_++;
      return Token::object_end;
    case '[':
      pos_++;
      return Token::array_start;
    case ']':
      pos_++;
      return Token::array_end;
    case '"': {
      size_t saved_pos = pos_;
      value_ = readString();
      if (value_.data() == nullptr) {
        pos_ = saved_pos;
        if (ended_) return Token::error;
        need_more_data_ = true;
        return Token::need_more_data;
      }
      // Peek ahead to see if this string is followed by a colon, marking it as
      // a key
      saved_pos = pos_;
      skipWhitespace();
      if (pos_ < json_.size() && json_[pos_] == ':') {
        return Token::key;
      }
      pos_ = saved_pos;  // Restore pos for next() to see the colon and skip it
                         // correctly
      return Token::string;
    }
    case 't':
      if (pos_ + 4 <= json_.size() && json_.substr(pos_, 4) == "true") {
        value_ = json_.substr(pos_, 4);
        pos_ += 4;
        return Token::boolean;
      }
      if (!ended_ && pos_ + 4 > json_.size()) {
        need_more_data_ = true;
        return Token::need_more_data;
      }
      return Token::error;
    case 'f':
      if (pos_ + 5 <= json_.size() && json_.substr(pos_, 5) == "false") {
        value_ = json_.substr(pos_, 5);
        pos_ += 5;
        return Token::boolean;
      }
      if (!ended_ && pos_ + 5 > json_.size()) {
        need_more_data_ = true;
        return Token::need_more_data;
      }
      return Token::error;
    case 'n':
      if (pos_ + 4 <= json_.size() && json_.substr(pos_, 4) == "null") {
        value_ = json_.substr(pos_, 4);
        pos_ += 4;
        return Token::null;
      }
      if (!ended_ && pos_ + 4 > json_.size()) {
        need_more_data_ = true;
        return Token::need_more_data;
      }
      return Token::error;
    default:
      if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
        size_t start = pos_;
        if (c == '-') pos_++;
        while (pos_ < json_.size() &&
               (std::isdigit(static_cast<unsigned char>(json_[pos_])) ||
                json_[pos_] == '.' || json_[pos_] == 'e' ||
                json_[pos_] == 'E' || json_[pos_] == '+' ||
                json_[pos_] == '-')) {
          pos_++;
        }
        // Check if number might be incomplete at buffer boundary
        if (!ended_ && pos_ >= json_.size()) {
          // Number might continue - need more data
          pos_ = start;
          need_more_data_ = true;
          return Token::need_more_data;
        }
        value_ = json_.substr(start, pos_ - start);
        return Token::number;
      }
      return Token::error;
  }
}

std::string_view JsonReader::rawValue() {
  if (rawvalue_in_progress_) {
    skipToClosing();
    if (need_more_data_) return {};
    rawvalue_in_progress_ = false;
    return json_.substr(rawvalue_start_, pos_ - rawvalue_start_);
  }

  skipWhitespace();
  if (pos_ < json_.size() && (json_[pos_] == ':' || json_[pos_] == ',')) {
    pos_++;
    skipWhitespace();
  }

  size_t start = pos_;
  Token t = next();
  if (t == Token::need_more_data) return {};
  if (t == Token::object_start || t == Token::array_start) {
    skipToClosing();
    if (need_more_data_) {
      rawvalue_start_ = start;
      rawvalue_in_progress_ = true;
      return {};
    }
  }
  if (t == Token::end || t == Token::error) {
    return {};
  }
  return json_.substr(start, pos_ - start);
}

bool JsonReader::findKey(std::string_view target_key) {
  int depth = 0;
  while (true) {
    Token t = next();
    if (t == Token::end || t == Token::error || t == Token::need_more_data)
      return false;

    if (t == Token::object_start || t == Token::array_start) {
      depth++;
    } else if (t == Token::object_end || t == Token::array_end) {
      depth--;
      if (depth < 0) return false;
    } else if (t == Token::key && depth == 0 && value_ == target_key) {
      return true;
    }
  }
}

void JsonReader::skipValue() {
  Token t = next();
  if (t == Token::need_more_data) return;
  if (t == Token::object_start || t == Token::array_start) {
    skipToClosing();
  }
}

void JsonReader::skipComposite(Token start_token) {
  if (start_token == Token::object_start || start_token == Token::array_start) {
    skipToClosing();
  }
}

bool JsonReader::validate(std::string_view json) {
  if (json.empty()) return false;

  char stack[JsonLimits::max_recursion_depth];
  int depth = -1;

  enum State {
    ExpectValue,
    ExpectKeyOrEnd,
    ExpectColon,
    ExpectCommaOrEnd
  } state = ExpectValue;

  size_t p = 0;
  auto skipWs = [&]() {
    p = json.find_first_not_of(" \t\n\r", p);
    if (p == std::string_view::npos) p = json.size();
  };

  skipWs();
  if (p == json.size()) return false;

  while (p < json.size()) {
    char c = json[p];
    switch (state) {
      case ExpectValue:
        if (c == '{') {
          if (++depth >= static_cast<int>(JsonLimits::max_recursion_depth))
            return false;
          stack[depth] = '{';
          p++;
          skipWs();
          if (p < json.size() && json[p] == '}') {
            state = ExpectCommaOrEnd;
          } else
            state = ExpectKeyOrEnd;
        } else if (c == '[') {
          if (++depth >= static_cast<int>(JsonLimits::max_recursion_depth))
            return false;
          stack[depth] = '[';
          p++;
          skipWs();
          if (p < json.size() && json[p] == ']') {
            state = ExpectCommaOrEnd;
          } else
            state = ExpectValue;
        } else if (c == '"') {
          p++;
          while (p < json.size() && json[p] != '"') {
            if (json[p] == '\\')
              p += 2;
            else
              p++;
          }
          if (p >= json.size()) return false;
          p++;
          state = ExpectCommaOrEnd;
        } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' ||
                   c == 't' || c == 'f' || c == 'n') {
          if (c == 't' || c == 'n') {
            if (json.substr(p, 4) != (c == 't' ? "true" : "null")) return false;
            p += 4;
          } else if (c == 'f') {
            if (json.substr(p, 5) != "false") return false;
            p += 5;
          } else {
            if (c == '-') p++;
            while (p < json.size() &&
                   (std::isdigit(static_cast<unsigned char>(json[p])) ||
                    std::strchr(".eE+-", json[p])))
              p++;
          }
          state = ExpectCommaOrEnd;
        } else
          return false;
        break;
      case ExpectKeyOrEnd:
        if (c == '}') {
          if (depth < 0 || stack[depth] != '{') return false;
          depth--;
          p++;
          state = ExpectCommaOrEnd;
        } else if (c == '"') {
          p++;
          while (p < json.size() && json[p] != '"') {
            if (json[p] == '\\')
              p += 2;
            else
              p++;
          }
          if (p >= json.size()) return false;
          p++;
          state = ExpectColon;
        } else
          return false;
        break;
      case ExpectColon:
        if (c != ':') return false;
        p++;
        state = ExpectValue;
        break;
      case ExpectCommaOrEnd:
        if (c == ',') {
          p++;
          skipWs();
          if (p >= json.size()) return false;
          // A comma must be followed by a key or a value, not a closing
          // delimiter
          if (json[p] == '}' || json[p] == ']') return false;
          if (depth >= 0 && stack[depth] == '{')
            state = ExpectKeyOrEnd;
          else
            state = ExpectValue;
          continue;
        } else if (c == '}') {
          if (depth < 0 || stack[depth] != '{') return false;
          depth--;
          p++;
          state = ExpectCommaOrEnd;
        } else if (c == ']') {
          if (depth < 0 || stack[depth] != '[') return false;
          depth--;
          p++;
          state = ExpectCommaOrEnd;
        } else
          return false;
        break;
    }
    skipWs();
  }
  return depth == -1 && state == ExpectCommaOrEnd;
}

JsonReader::Token JsonReader::get(std::string_view path) {
  // Reset the reader's state to start parsing from the beginning
  reset();

  Token current_token = next();  // Get the root container token
  if (current_token == Token::need_more_data) return Token::error;
  if (current_token != Token::object_start &&
      current_token != Token::array_start) {
    return Token::error;  // JSON must start with an object or array
  }
  Token current_container_type = current_token;

  size_t start = 0;
  while (true) {
    size_t end = path.find('.', start);  // Find the next path segment separator
    std::string_view segment = (end == std::string_view::npos)
                                   ? path.substr(start)
                                   : path.substr(start, end - start);

    if (current_container_type == Token::object_start) {
      // In an object, expect a key
      if (!findKey(segment)) {
        return Token::error;  // Key not found
      }
      // After findKey, the reader is positioned before the value. Get the
      // value's token.
      Token value_token = next();
      if (value_token == Token::need_more_data) return Token::error;
      if (end == std::string_view::npos) {
        return value_token;  // This is the final value
      }
      // Path continues, so the value must be a container
      if (value_token != Token::object_start &&
          value_token != Token::array_start) {
        return Token::error;  // Expected object or array, got primitive
      }
      current_container_type = value_token;  // Update current container type
    } else if (current_container_type == Token::array_start) {
      // In an array, expect a numeric index
      std::optional<size_t> index_opt = parseIndex(segment);
      if (!index_opt) {
        return Token::error;  // Invalid array index in path segment
      }

      size_t target_index = *index_opt;
      size_t current_index = 0;
      while (current_index < target_index) {
        skipValue();
        current_index++;
      }

      Token element_token = next();
      if (element_token == Token::array_end || element_token == Token::end ||
          element_token == Token::error ||
          element_token == Token::need_more_data) {
        return Token::error;
      }

      if (end == std::string_view::npos) {
        return element_token;
      }

      if (element_token != Token::object_start &&
          element_token != Token::array_start) {
        return Token::error;
      }
      current_container_type = element_token;
    }

    else {
      return Token::error;
    }

    if (end == std::string_view::npos) {
      break;  // Reached the end of the path
    }
    start = end + 1;  // Move past the dot for the next segment
  }

  // If the loop finishes, it means the last segment was processed and its value
  // token was returned. This return is effectively unreachable if the path is
  // valid and ends with a value. The last `return value_token` or `return
  // element_token` handles the final value.
  return Token::error;  // Should not reach here
}

void JsonReader::feed(std::string_view chunk) {
  if (!streaming_mode_) {
    return;
  }

  // Fixed external buffer (ring buffer with memmove compaction)
  if (fixed_buffer_.data()) {
    size_t buf_end = buffer_pos_ + json_.size();
    size_t available = fixed_buffer_.size() - buf_end;
    if (chunk.size() > available) {
      // Need to compact - discard consumed data
      // When rawValue is in progress, preserve from rawvalue_start_ (the '{')
      size_t discard_to = pos_;
      if (rawvalue_in_progress_ && rawvalue_start_ < discard_to) {
        discard_to = rawvalue_start_;
      }
      size_t consumed = buffer_pos_ + discard_to;
      size_t unread = json_.size() - discard_to;
      if (consumed > 0) {
        if (unread > 0) {
          memmove(const_cast<char*>(fixed_buffer_.data()),
                  const_cast<char*>(fixed_buffer_.data()) + consumed, unread);
        }
        buffer_pos_ = 0;
        json_ = std::string_view(fixed_buffer_.data(), unread);
        pos_ -= discard_to;
        if (rawvalue_in_progress_) {
          rawvalue_start_ -= discard_to;
        }
      }
      available = fixed_buffer_.size() - json_.size();
      if (chunk.size() > available) {
        need_more_data_ = true;
        return;
      }
    }
    size_t write_pos = buffer_pos_ + json_.size();
    memcpy(const_cast<char*>(fixed_buffer_.data()) + write_pos, chunk.data(),
           chunk.size());
    json_ = std::string_view(fixed_buffer_.data() + buffer_pos_,
                             json_.size() + chunk.size());
    need_more_data_ = false;
    return;
  }

  // Dynamic buffer (std::string)
  if (buffer_pos_ > buffer_.size() / 2 && buffer_pos_ > 64) {
    buffer_.erase(0, buffer_pos_);
    buffer_pos_ = 0;
    json_ = std::string_view(buffer_);
  }
  buffer_.append(chunk);
  json_ = std::string_view(buffer_).substr(buffer_pos_);
  need_more_data_ = false;
}

void JsonReader::endOfInput() {
  ended_ = true;
  if (rawvalue_in_progress_) {
    need_more_data_ = true;
  } else {
    need_more_data_ = false;
  }
}

void JsonReader::skipWhitespace() {
  pos_ = json_.find_first_not_of(" \t\n\r", pos_);
  if (pos_ == std::string_view::npos) pos_ = json_.size();
}

void JsonReader::skipToClosing() {
  int depth = skip_to_closing_depth_;
  bool in_string = skip_to_closing_in_string_;
  while (pos_ < json_.size() && depth > 0) {
    char c = json_[pos_];
    if (in_string) {
      if (c == '\\') {
        if (pos_ + 1 >= json_.size()) {
          if (!ended_) {
            need_more_data_ = true;
            skip_to_closing_in_string_ = true;
            skip_to_closing_depth_ = depth;
            return;
          }
          pos_ = json_.size();
          return;
        }
        pos_ += 2;
      } else if (c == '"') {
        in_string = false;
        pos_++;
      } else {
        pos_++;
      }
    } else {
      if (c == '"') {
        in_string = true;
        pos_++;
      } else if (c == '{' || c == '[') {
        depth++;
        pos_++;
      } else if (c == '}' || c == ']') {
        depth--;
        pos_++;
      } else {
        pos_++;
      }
    }
  }
  if (depth > 0) {
    need_more_data_ = true;
    skip_to_closing_in_string_ = in_string;
    skip_to_closing_depth_ = depth;
    return;
  }
  skip_to_closing_in_string_ = false;
  skip_to_closing_depth_ = 1;
  need_more_data_ = false;
}

std::string_view JsonReader::readString() {
  if (pos_ >= json_.size() || json_[pos_] != '"') return {};
  size_t start = ++pos_;
  while (pos_ < json_.size()) {
    if (json_[pos_] == '\\') {
      if (pos_ + 1 >= json_.size()) {
        if (ended_) return {};
        return {};  // Incomplete escape sequence
      }
      char next_ch = json_[pos_ + 1];
      if (next_ch == 'u') {
        // Unicode escape \uXXXX - need 6 chars total (including backslash)
        if (pos_ + 5 >= json_.size()) {
          if (ended_) return {};
          return {};  // Incomplete Unicode escape
        }
        pos_ += 6;
      } else {
        // Simple escape sequences: \", \\, \/, \b, \f, \n, \r, \t
        pos_ += 2;
      }
    } else if (json_[pos_] == '"') {
      std::string_view s = json_.substr(start, pos_ - start);
      pos_++;
      return s;
    } else {
      pos_++;
    }
  }
  // Reached end of buffer without finding closing quote
  if (ended_) return {};
  return {};
}

std::optional<size_t> JsonReader::parseIndex(std::string_view s) {
  size_t index = 0;
  // Use std::from_chars for zero-allocation parsing
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), index);
  if (ec == std::errc{} && ptr == s.data() + s.size()) {
    return index;
  }
  return std::nullopt;
}

}  // namespace ebus::detail
