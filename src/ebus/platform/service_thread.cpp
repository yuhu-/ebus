/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform/service_thread.hpp"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#elif defined(POSIX)
#include <thread>
#endif

namespace ebus::detail {

struct ServiceThread::Impl {
  std::string name;
  std::function<void()> func;
  uint32_t stack_size;
  uint8_t priority;
  int core;
#if defined(ESP32)
  TaskHandle_t handle = nullptr;
  SemaphoreHandle_t done_sem = nullptr;
#elif defined(POSIX)
  std::thread thread{};
#endif
};

ServiceThread::ServiceThread(std::string name, std::function<void()> func,
                             uint32_t stack_size, uint8_t priority, int core)
    : impl_(new Impl{name, std::move(func), stack_size, priority, core}) {
#if defined(ESP32)
  impl_->done_sem = xSemaphoreCreateBinary();
#endif
}

ServiceThread::~ServiceThread() {
  join();
#if defined(ESP32)
  if (impl_->done_sem) vSemaphoreDelete(impl_->done_sem);
#endif
}

void ServiceThread::start() {
#if defined(ESP32)
  xTaskCreatePinnedToCore(
      [](void* arg) {
        auto* impl = static_cast<Impl*>(arg);
        impl->func();
        xSemaphoreGive(impl->done_sem);
        impl->handle = nullptr;
        vTaskDelete(NULL);
      },
      impl_->name.c_str(), impl_->stack_size, impl_.get(), impl_->priority,
      &impl_->handle, (impl_->core >= 0) ? impl_->core : tskNO_AFFINITY);
#elif defined(POSIX)
  if (impl_->thread.joinable()) impl_->thread.join();
  impl_->thread = std::thread(impl_->func);
#endif
}

void ServiceThread::join() {
#if defined(ESP32)
  if (impl_->handle) {
    // Wait for task to finish or timeout
    xSemaphoreTake(impl_->done_sem, pdMS_TO_TICKS(2000));
  }
#elif defined(POSIX)
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
#endif
}

}  // namespace ebus::detail