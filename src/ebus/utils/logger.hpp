/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <ebus/types.hpp>
#include <functional>
#include <string>
#include <utility>

namespace ebus::detail {

/**
 * Internal logger used throughout the library to provide diagnostics.
 * Decouples logging logic from specific services and provides a central
 * point for debug output.
 */
class Logger {
 public:
  using LogSink = std::function<void(LogLevel level, const std::string& msg)>;

  static void setLevel(LogLevel level) { level_ = level; }
  static void setSink(LogSink sink) { sink_ = std::move(sink); }

  static void log(LogLevel level, const std::string& msg) {
    if (level == LogLevel::none) return;
    if (level <= level_ && sink_) {
      sink_(level, msg);
    }
  }

  static void error(const std::string& msg) { log(LogLevel::error, msg); }
  static void info(const std::string& msg) { log(LogLevel::info, msg); }
  static void debug(const std::string& msg) { log(LogLevel::debug, msg); }

  static bool isEnabled(LogLevel level) {
    if (level == LogLevel::none) return false;
    return level <= level_;
  }

 private:
  inline static LogLevel level_ = LogLevel::error;
  inline static LogSink sink_ = nullptr;
};

}  // namespace ebus::detail
