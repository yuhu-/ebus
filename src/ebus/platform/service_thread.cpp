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

struct ebus::ServiceThread::Impl {
  std::string name_;
  std::function<void()> func_;
  uint32_t stack_size_;
  uint8_t priority_;
  int core_;
#if defined(ESP32)
  TaskHandle_t handle = nullptr;
  SemaphoreHandle_t done_sem = nullptr;
#elif defined(POSIX)
  std::thread thread{};
#endif
};

ebus::ServiceThread::ServiceThread(std::string name, std::function<void()> func,
                                   uint32_t stackSize, uint8_t priority,
                                   int core)
    : impl_(new Impl{name, std::move(func), stackSize, priority, core}) {
#if defined(ESP32)
  impl_->done_sem = xSemaphoreCreateBinary();
#endif
}

ebus::ServiceThread::~ServiceThread() {
  join();
#if defined(ESP32)
  if (impl_->done_sem) vSemaphoreDelete(impl_->done_sem);
#endif
}

void ebus::ServiceThread::start() {
#if defined(ESP32)
  xTaskCreatePinnedToCore(
      [](void* arg) {
        auto* impl = static_cast<Impl*>(arg);
        impl->func_();
        xSemaphoreGive(impl->done_sem);
        impl->handle = nullptr;
        vTaskDelete(NULL);
      },
      impl_->name_.c_str(), impl_->stack_size_, impl_.get(), impl_->priority_,
      &impl_->handle, (impl_->core_ >= 0) ? impl_->core_ : tskNO_AFFINITY);
#elif defined(POSIX)
  if (impl_->thread.joinable()) impl_->thread.join();
  impl_->thread = std::thread(impl_->func_);
#endif
}

void ebus::ServiceThread::join() {
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
