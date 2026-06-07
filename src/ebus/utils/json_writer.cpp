/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/detail/json_writer.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

void writeEscapedJson(std::string_view s, const JsonChunkVisitor& visitor) {
  size_t start = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    const char* escaped = nullptr;
    switch (c) {
      case '"':
        escaped = "\\\"";
        break;
      case '\\':
        escaped = "\\\\";
        break;
      case '\b':
        escaped = "\\b";
        break;
      case '\f':
        escaped = "\\f";
        break;
      case '\n':
        escaped = "\\n";
        break;
      case '\r':
        escaped = "\\r";
        break;
      case '\t':
        escaped = "\\t";
        break;
      default:
        break;
    }

    if (escaped) {
      if (i > start) visitor(s.substr(start, i - start));
      visitor(escaped);
      start = i + 1;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      if (i > start) visitor(s.substr(start, i - start));
      static const char hex[] = "0123456789abcdef";
      char hex_buf[6] = {'\\',
                         'u',
                         '0',
                         '0',
                         hex[(static_cast<uint8_t>(c) >> 4) & 0xf],
                         hex[static_cast<uint8_t>(c) & 0xf]};
      visitor(std::string_view(hex_buf, 6));
      start = i + 1;
    }
  }
  if (start < s.length()) visitor(s.substr(start));
}

void appendHexFieldToWriter(JsonWriter& writer, ByteView data) {
  static constexpr char hex_chars[] = "0123456789abcdef";
  char buf[64];
  size_t buf_pos = 0;

  for (uint8_t b : data) {
    buf[buf_pos++] = hex_chars[b >> 4];
    buf[buf_pos++] = hex_chars[b & 0xf];
    if (buf_pos == sizeof(buf)) {
      writer.write(std::string_view(buf, buf_pos));
      buf_pos = 0;
    }
  }
  if (buf_pos > 0) {
    writer.write(std::string_view(buf, buf_pos));
  }
}

void JsonWriter::writeTimestampField(std::string_view key, uint64_t ms) {
  appendKey(key);
  char iso_buffer[26];
  ebus::formatIso8601Fast(ms, iso_buffer);
  write("\"");
  write(std::string_view(iso_buffer));
  write("\"");
  first_ = false;
}

}  // namespace ebus::detail
