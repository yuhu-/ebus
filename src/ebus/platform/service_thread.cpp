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
  std::string name;
  std::function<void()> func;
  uint32_t stack;
  uint8_t prio;
  int core;
#if defined(ESP32)
  TaskHandle_t handle = nullptr;
  SemaphoreHandle_t doneSem = nullptr;
#elif defined(POSIX)
  std::thread thread{};
#endif
};

ebus::ServiceThread::ServiceThread(std::string name, std::function<void()> func,
                                   uint32_t stackSize, uint8_t priority,
                                   int core)
    : impl_(new Impl{name, std::move(func), stackSize, priority, core}) {
#if defined(ESP32)
  impl_->doneSem = xSemaphoreCreateBinary();
#endif
}

ebus::ServiceThread::~ServiceThread() {
  join();
#if defined(ESP32)
  if (impl_->doneSem) vSemaphoreDelete(impl_->doneSem);
#endif
}

void ebus::ServiceThread::start() {
#if defined(ESP32)
  xTaskCreatePinnedToCore(
      [](void* arg) {
        auto* impl = static_cast<Impl*>(arg);
        impl->func();
        xSemaphoreGive(impl->doneSem);
        impl->handle = nullptr;
        vTaskDelete(NULL);
      },
      impl_->name.c_str(), impl_->stack, impl_.get(), impl_->prio,
      &impl_->handle, (impl_->core >= 0) ? impl_->core : tskNO_AFFINITY);
#elif defined(POSIX)
  if (impl_->thread.joinable()) impl_->thread.join();
  impl_->thread = std::thread(impl_->func);
#endif
}

void ebus::ServiceThread::join() {
#if defined(ESP32)
  if (impl_->handle) {
    // Wait for task to finish or timeout
    xSemaphoreTake(impl_->doneSem, pdMS_TO_TICKS(2000));
  }
#elif defined(POSIX)
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
#endif
}
