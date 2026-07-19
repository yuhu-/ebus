/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if __has_include(<esp_rom_sys.h>)
#include <esp_rom_sys.h>
#else
#include <rom/ets_sys.h>
#endif
#elif defined(POSIX)
#include <chrono>
#include <future>
#include <thread>
#endif

namespace ebus::detail::platform {

/**
 * Suspends execution for a given number of milliseconds.
 */
inline void sleepMilli(uint32_t ms) {
#if defined(ESP_PLATFORM)
  vTaskDelay(pdMS_TO_TICKS(ms));
#elif defined(POSIX)
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

/**
 * Suspends execution for a given number of microseconds.
 */
inline void sleepMicro(uint32_t us) {
#if defined(ESP_PLATFORM)
  esp_rom_delay_us(us);
#elif defined(POSIX)
  std::this_thread::sleep_for(std::chrono::microseconds(us));
#endif
}

#if defined(POSIX)
class AsyncOneShotTimer {
 public:
  AsyncOneShotTimer(std::chrono::microseconds delay,
                    std::vector<std::function<void()>> callbacks)
      : delay_(delay), callbacks_(std::move(callbacks)) {}

  void arm() {
    [[maybe_unused]] auto future = std::async(std::launch::async, [this]() {
      std::this_thread::sleep_for(delay_);
      for (const auto& callback : callbacks_) {
        callback();
      }
    });
  }

 private:
  std::chrono::microseconds delay_;
  std::vector<std::function<void()>> callbacks_;
};
#endif

}  // namespace ebus::detail::platform
