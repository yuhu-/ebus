/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <charconv>
#include <cmath>
#include <cstring>
#include <ctime>
#include <ebus/detail/json_writer.hpp>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>

namespace ebus {

namespace {
static constexpr char hex_lookup_table[256][3] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b",
    "0c", "0d", "0e", "0f", "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19", "1a", "1b", "1c", "1d", "1e", "1f", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3a", "3b",
    "3c", "3d", "3e", "3f", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "4a", "4b", "4c", "4d", "4e", "4f", "50", "51", "52", "53",
    "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e", "5f",
    "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b",
    "6c", "6d", "6e", "6f", "70", "71", "72", "73", "74", "75", "76", "77",
    "78", "79", "7a", "7b", "7c", "7d", "7e", "7f", "80", "81", "82", "83",
    "84", "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9a", "9b",
    "9c", "9d", "9e", "9f", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af", "b0", "b1", "b2", "b3",
    "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf",
    "c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb",
    "cc", "cd", "ce", "cf", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "d8", "d9", "da", "db", "dc", "dd", "de", "df", "e0", "e1", "e2", "e3",
    "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "fa", "fb",
    "fc", "fd", "fe", "ff"};
}  // namespace

std::string escapeJson(std::string_view s) {
  std::string res;
  res.reserve(s.length());
  appendEscapedJson(res, s);
  return res;
}

void writeEscapedJson(std::string_view s, const JsonChunkVisitor& visitor) {
  detail::writeEscapedJson(s, visitor);
}

void appendEscapedJson(std::string& out, std::string_view s) {
  writeEscapedJson(s, [&out](std::string_view chunk) { out.append(chunk); });
}

const char* toString(uint8_t byte) { return hex_lookup_table[byte]; }

void byteToChar(std::string& out, ByteView data) {
  out.append(reinterpret_cast<const char*>(data.data()), data.size());
}

void appendHex(std::string& out, ByteView data) {
  for (auto b : data) {
    const char* hex = hex_lookup_table[b];
    out.push_back(hex[0]);
    out.push_back(hex[1]);
  }
}

std::vector<uint8_t> toVector(const std::string& str) {
  if (str.empty()) return {};
  std::vector<uint8_t> result;
  result.reserve(str.size() / 2);
  for (size_t i = 0; i + 1 < str.size(); i += 2) {
    uint8_t value = 0;
    auto [ptr, ec] =
        std::from_chars(str.data() + i, str.data() + i + 2, value, 16);
    if (ec == std::errc{}) result.push_back(value);
  }
  return result;
}

void formatIso8601Fast(uint64_t ms_since_epoch, char* out) {
  time_t seconds = static_cast<time_t>(ms_since_epoch / 1000);
  int ms = static_cast<int>(ms_since_epoch % 1000);
  struct tm tm_info;
  gmtime_r(&seconds, &tm_info);

  auto write_2d = [](int val, char* p) {
    p[0] = static_cast<char>('0' + (val / 10));
    p[1] = static_cast<char>('0' + (val % 10));
  };

  int year = tm_info.tm_year + 1900;
  out[0] = static_cast<char>('0' + (year / 1000));
  out[1] = static_cast<char>('0' + ((year / 100) % 10));
  out[2] = static_cast<char>('0' + ((year / 10) % 10));
  out[3] = static_cast<char>('0' + (year % 10));
  out[4] = '-';
  write_2d(tm_info.tm_mon + 1, out + 5);
  out[7] = '-';
  write_2d(tm_info.tm_mday, out + 8);
  out[10] = 'T';
  write_2d(tm_info.tm_hour, out + 11);
  out[13] = ':';
  write_2d(tm_info.tm_min, out + 14);
  out[16] = ':';
  write_2d(tm_info.tm_sec, out + 17);
  out[19] = '.';
  out[20] = static_cast<char>('0' + (ms / 100));
  out[21] = static_cast<char>('0' + ((ms / 10) % 10));
  out[22] = static_cast<char>('0' + (ms % 10));
  out[23] = 'Z';
  out[24] = '\0';
}

char* formatFloat(float value, int precision, char* buffer, size_t buffer_size,
                  float lower_threshold, float upper_threshold) {
  if (buffer_size == 0) return buffer;
  buffer[0] = '\0';  // Ensure buffer is empty initially

  if (std::isnan(value)) {
    std::strncpy(buffer, "null", buffer_size);
    buffer[buffer_size - 1] = '\0';
    return buffer + std::min(buffer_size - 1, (size_t)4);
  }
  if (std::isinf(value)) {
    if (value < 0) {
      std::strncpy(buffer, "-inf", buffer_size);
      buffer[buffer_size - 1] = '\0';
      return buffer + std::min(buffer_size - 1, (size_t)4);
    } else {
      std::strncpy(buffer, "inf", buffer_size);
      buffer[buffer_size - 1] = '\0';
      return buffer + std::min(buffer_size - 1, (size_t)3);
    }
  }

  size_t current_len = 0;
  bool success = false;

#if !defined(ESP_PLATFORM)
  // Attempt std::to_chars for platforms where it's fully supported for floats
  if ((std::abs(value) > 0 && std::abs(value) < lower_threshold) ||
      std::abs(value) >= upper_threshold) {
    auto res = std::to_chars(buffer, buffer + buffer_size, value,
                             std::chars_format::scientific, precision);
    if (res.ec == std::errc{}) {
      current_len = res.ptr - buffer;
      success = true;
    }
  } else {
    auto res = std::to_chars(buffer, buffer + buffer_size, value,
                             std::chars_format::fixed, precision);
    if (res.ec == std::errc{}) {
      current_len = res.ptr - buffer;
      success = true;
    }
  }
#endif

  if (!success) {
    // Fallback for ESP32 or if to_chars failed: use snprintf.
    // Use literals directly to avoid "format-nonliteral" warnings.
    int n;
    if (((std::abs(value) > 0 && std::abs(value) < lower_threshold) ||
         std::abs(value) >= upper_threshold)) {
      n = std::snprintf(buffer, buffer_size, "%.*e", precision, value);
    } else {
      n = std::snprintf(buffer, buffer_size, "%.*f", precision, value);
    }
    if (n > 0 && static_cast<size_t>(n) < buffer_size) {
      current_len = static_cast<size_t>(n);
      success = true;
    }
  }

  if (success) {
    if (current_len == 0) {
      std::strncpy(buffer, "0", buffer_size);
      buffer[buffer_size - 1] = '\0';
      return buffer + 1;
    }

    // Post-processing: remove trailing zeros and decimal point if no fractional
    // part
    bool is_scientific = false;
    for (size_t i = 0; i < current_len; ++i) {
      if (buffer[i] == 'e' || buffer[i] == 'E') {
        is_scientific = true;
        break;
      }
    }

    if (!is_scientific) {
      size_t decimal_pos = current_len;
      bool has_decimal = false;
      for (size_t i = 0; i < current_len; ++i) {
        if (buffer[i] == '.') {
          has_decimal = true;
          decimal_pos = i;
          break;
        }
      }

      if (has_decimal) {
        // Remove trailing zeros
        while (current_len > decimal_pos + 1 &&
               buffer[current_len - 1] == '0') {
          current_len--;
        }
        // Remove trailing decimal point if no digits follow
        if (current_len > decimal_pos && buffer[current_len - 1] == '.') {
          current_len--;
        }
      }
    }

    // Ensure null termination and return end pointer
    buffer[current_len] = '\0';
    return buffer + current_len;
  }

  std::strncpy(buffer, "ERR_FLOAT_FORMAT", buffer_size);
  buffer[buffer_size - 1] = '\0';
  return buffer + std::min(buffer_size - 1, (size_t)16);
}

Sequence frameMaster(uint8_t source, ByteView payload) {
  Sequence msg;
  msg.pushBack(source, false);
  msg.append(makeSequence(payload));
  msg.pushBack(msg.crc(), false);
  msg.extend();
  return msg;
}

std::string frameMasterHex(uint8_t source, ByteView payload) {
  return byteToHex(frameMaster(source, payload));
}

std::string frameMasterHex(uint8_t source, const std::string& payload) {
  return frameMasterHex(source, toVector(payload));
}

Sequence frameSlave(ByteView payload) {
  Sequence msg;
  msg.append(makeSequence(payload));
  msg.pushBack(msg.crc(), false);
  msg.extend();
  return msg;
}

std::string frameSlaveHex(ByteView payload) {
  return byteToHex(frameSlave(payload));
}

std::string frameSlaveHex(const std::string& payload) {
  return frameSlaveHex(toVector(payload));
}

float roundDigits(float value, uint8_t digits) noexcept {
  float decimals = std::pow(10.0f, static_cast<float>(digits));
  return std::round(value * decimals) / decimals;
}

/**
 * Finds a key safely by ensuring it is wrapped in quotes and followed by a
 * colon.
 */
size_t findKey(std::string_view json, std::string_view key) {
  size_t pos = 0;
  while ((pos = json.find('"', pos)) != std::string_view::npos) {
    std::string_view sub = json.substr(pos + 1);
    if (sub.size() > key.size() && sub.compare(0, key.size(), key) == 0 &&
        sub[key.size()] == '"') {
      size_t colon = json.find(':', pos + key.size() + 2);
      if (colon != std::string_view::npos) return colon + 1;
    }
    pos++;
  }
  return std::string_view::npos;
}

std::string_view extract(std::string_view json, std::string_view key) {
  size_t pos = findKey(json, key);
  if (pos == std::string_view::npos) return {};

  size_t start = json.find_first_not_of(" \t\n\r", pos);
  if (start == std::string_view::npos) return {};

  size_t end;
  if (json[start] == '"') {
    start++;
    // Basic robustness: handle escaped quotes \" by looking ahead
    end = start;
    while ((end = json.find('"', end)) != std::string_view::npos) {
      if (json[end - 1] != '\\') break;
      end++;
    }
  } else {
    end = json.find_first_of(", \t\n\r}", start);
  }
  return (end == std::string_view::npos) ? json.substr(start)
                                         : json.substr(start, end - start);
}

std::string_view extractSub(std::string_view json, std::string_view key) {
  size_t pos = findKey(json, key);
  if (pos == std::string_view::npos) return {};

  size_t start = json.find('{', pos);
  if (start == std::string_view::npos) return {};
  int depth = 0;
  for (size_t i = start; i < json.size(); ++i) {
    if (json[i] == '{')
      depth++;
    else if (json[i] == '}')
      depth--;
    if (depth == 0) return json.substr(start, i - start + 1);
  }
  return "";
}

}  // namespace ebus
