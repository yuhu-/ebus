/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "ebus/types.hpp"

namespace ebus {

// --- Parsing Helpers ---

size_t findKey(std::string_view json, std::string_view key);
std::string_view extract(std::string_view json, std::string_view key);
std::string_view extractSub(std::string_view json, std::string_view key);

template <typename T>
T toNum(std::string_view s) {
  if (s.empty() || s == "null") return 0;
  T val = 0;
  std::from_chars(s.data(), s.data() + s.size(), val);
  return val;
}

// --- Structural Helpers (Deduplicated in Flash) ---

void append_key(std::string& json, const char* key, bool& first);
void append_field(std::string& json, const char* key, std::string_view val,
                  bool& first);
void append_field(std::string& json, const char* key, bool val, bool& first);
void append_field(std::string& json, const char* key, int64_t val, bool& first);
void append_field(std::string& json, const char* key, uint64_t val,
                  bool& first);
void append_field(std::string& json, const char* key, double val, bool& first);

/**
 * Appends data as a hex string to the provided buffer.
 */
void appendHex(std::string& out, ByteView data);

/**
 * Small template specialized for Enums to minimize monomorphization bloat.
 */
template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
void append_enum_field(std::string& json, const char* key, E val, bool& first) {
  append_field(json, key, std::string_view(toString(val)), first);
}

void append_hex_field(std::string& json, const char* key, ByteView data,
                      bool& first);

void append_json_timestamp(std::string& json_str, const char* key,
                           uint64_t timestamp_ms, bool& first_field);

void append_json_raw(std::string& json_str, const char* key,
                     const std::string& raw_json, bool& first_field);

void append_json_value_raw(std::string& json_str, const char* key,
                           const char* raw_value, bool& first_field);

void append_json_float_custom_precision(std::string& json_str, const char* key,
                                        float value, int precision,
                                        bool& first_field);

}  // namespace ebus
