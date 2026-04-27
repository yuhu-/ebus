/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/detail/protocol_limits.hpp>
#include <functional>
#include <memory>
#include <string>

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#elif defined(POSIX)
#include <thread>
#endif

namespace ebus::detail::platform {

/**
 * Platform-independent abstraction for a background worker.
 * Wraps std::thread on POSIX and FreeRTOS tasks on ESP_PLATFORM.
 */
class ServiceThread {
 public:
  ServiceThread(std::string name, std::function<void()> func,
                uint32_t stack_size = OrchestrationLimits::stack_size,
                uint8_t priority = OrchestrationLimits::priority_low,
                int core = -1);
  ~ServiceThread() { join(); }

  void start() {
#if defined(ESP_PLATFORM)
    if (done_sem_ == nullptr) done_sem_ = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(
        [](void* arg) {
          auto* self = static_cast<ServiceThread*>(arg);
          self->func_();
          xSemaphoreGive(self->done_sem_);
          self->handle_ = nullptr;
          vTaskDelete(NULL);
        },
        name_.c_str(), stack_size_, this, priority_, &handle_,
        (core_ >= 0) ? core_ : tskNO_AFFINITY);
#elif defined(POSIX)
    if (thread_.joinable()) thread_.join();
    thread_ = std::thread(func_);
#endif
  }

  void join() {
#if defined(ESP_PLATFORM)
    if (handle_ && done_sem_) {
      xSemaphoreTake(
          done_sem_,
          pdMS_TO_TICKS(OrchestrationLimits::termination_timeout_ms));
      vSemaphoreDelete(done_sem_);
      done_sem_ = nullptr;
    }
#elif defined(POSIX)
    if (thread_.joinable()) {
      thread_.join();
    }
#endif
  }

 private:
  std::string name_;
  std::function<void()> func_;
  uint32_t stack_size_;
  uint8_t priority_;
  int core_;

#if defined(ESP_PLATFORM)
  TaskHandle_t handle_ = nullptr;
  SemaphoreHandle_t done_sem_ = nullptr;
#elif defined(POSIX)
  std::thread thread_{};
#endif
};

inline ServiceThread::ServiceThread(std::string name,
                                    std::function<void()> func,
                                    uint32_t stack_size, uint8_t priority,
                                    int core)
    : name_(std::move(name)),
      func_(std::move(func)),
      stack_size_(stack_size),
      priority_(priority),
      core_(core) {}

}  // namespace ebus::detail::platform
