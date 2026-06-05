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
#include <string_view>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_system.h>
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
  // Public Types & Constants
  struct Status {
    // -1 if not available
    std::string_view name;
    int32_t task_stack_bytes;       // configured stack size in bytes
    int32_t task_stack_free_bytes;  // free stack (high-water) in bytes
  };

  // Lifecycle
  ServiceThread(std::string name, std::function<void()> func,
                uint32_t stack_size = OrchestrationLimits::default_stack_size,
                uint8_t priority = OrchestrationLimits::default_priority,
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

  // Status/Telemetry
  inline Status status() const {
    Status s;
    s.name = name_;
#if defined(ESP_PLATFORM)
    // configured stack bytes
    s.task_stack_bytes =
        static_cast<int32_t>(stack_size_ * sizeof(StackType_t));
    // if task handle exists, get high water mark (words) -> bytes.
    if (handle_) {
      UBaseType_t words_free = uxTaskGetStackHighWaterMark(handle_);
      s.task_stack_free_bytes =
          static_cast<int32_t>(words_free * sizeof(StackType_t));
    } else {
      // task not created yet; report full configured stack as free
      s.task_stack_free_bytes = s.task_stack_bytes;
    }
#else
    s.task_stack_bytes = -1;
    s.task_stack_free_bytes = -1;
#endif
    return s;
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
