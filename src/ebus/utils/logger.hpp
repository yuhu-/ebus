/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <ebus/types.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace ebus::detail {

/**
 * Thread-safe singleton logger used throughout the library.
 */
class Logger {
 public:
  using LogSink = std::function<void(LogLevel level, std::string_view msg)>;

  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  void setLevel(LogLevel level) {
    if (level_.load(std::memory_order_relaxed) == level) return;
    level_.store(level, std::memory_order_relaxed);
  }

  LogLevel getLevel() const { return level_.load(std::memory_order_relaxed); }

  void setSink(LogSink sink) {
    auto new_sink = std::make_shared<LogSink>(std::move(sink));
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ptr_ = std::move(new_sink);
  }

  /**
   * @brief Logs a message if the level is enabled.
   * @note The LogSink is called synchronously. The caller must ensure
   * the sink is non-blocking to protect eBUS protocol timing.
   */
  void log(LogLevel level, std::string_view msg) {
    if (!isEnabled(level)) return;

    std::shared_ptr<LogSink> current_sink;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      current_sink = sink_ptr_;
    }

    if (current_sink && *current_sink) {
      (*current_sink)(level, msg);
    }
  }

  bool isEnabled(LogLevel level) const {
    if (level == LogLevel::none) return false;
    return level <= level_.load(std::memory_order_relaxed);
  }

 private:
  Logger() = default;
  std::atomic<LogLevel> level_{LogLevel::error};
  std::shared_ptr<LogSink> sink_ptr_;
  std::mutex mutex_;
};

}  // namespace ebus::detail

/**
 * Logging macros allow for complete compile-time stripping and
 * prevent string construction overhead when the level is disabled.
 */
#ifndef EBUS_DISABLE_LOGGING
#define EBUS_LOG_ERROR(msg)                                              \
  do {                                                                   \
    if (::ebus::detail::Logger::getInstance().isEnabled(                 \
            ::ebus::LogLevel::error))                                    \
      ::ebus::detail::Logger::getInstance().log(::ebus::LogLevel::error, \
                                                (msg));                  \
  } while (0)
#define EBUS_LOG_INFO(msg)                                              \
  do {                                                                  \
    if (::ebus::detail::Logger::getInstance().isEnabled(                \
            ::ebus::LogLevel::info))                                    \
      ::ebus::detail::Logger::getInstance().log(::ebus::LogLevel::info, \
                                                (msg));                 \
  } while (0)
#define EBUS_LOG_DEBUG(msg)                                              \
  do {                                                                   \
    if (::ebus::detail::Logger::getInstance().isEnabled(                 \
            ::ebus::LogLevel::debug))                                    \
      ::ebus::detail::Logger::getInstance().log(::ebus::LogLevel::debug, \
                                                (msg));                  \
  } while (0)
#else
#define EBUS_LOG_ERROR(msg) ((void)0)
#define EBUS_LOG_INFO(msg) ((void)0)
#define EBUS_LOG_DEBUG(msg) ((void)0)
#endif
