/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <chrono>

namespace ebus {

template <typename T>
class QueueFreeRtos {
 public:
  explicit QueueFreeRtos(size_t capacity = 32)
      : queue_(xQueueCreate(capacity, sizeof(T))), capacity_(capacity) {}

  ~QueueFreeRtos() {
    if (queue_) vQueueDelete(queue_);
  }

  // Blocking push with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool try_push_for(const T& item, std::chrono::duration<Rep, Period> timeout) {
    return push(item, timeout);
  }

  // Blocking push (move semantics) with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool try_push_for(T&& item, std::chrono::duration<Rep, Period> timeout) {
    return push(std::move(item), timeout);
  }

  // Blocking push with duration
  template <typename Rep, typename Period>
  bool push(const T& item, std::chrono::duration<Rep, Period> timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    return push(item, static_cast<uint32_t>(ms.count()));
  }

  // Blocking push with timeout in milliseconds
  bool push(const T& item, uint32_t timeout_ms = portMAX_DELAY) {
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue_, &item, ticks) == pdTRUE;
  }

  // Blocking push (move semantics) with timeout in milliseconds
  bool push(T&& item, uint32_t timeout_ms = portMAX_DELAY) {
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue_, &item, ticks) == pdTRUE;
  }

  // Non-blocking push
  bool try_push(const T& item) {
    return xQueueSend(queue_, &item, 0) == pdTRUE;
  }

  // ISR-safe push (from ISR context)
  bool pushFromISR(const T& item) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueSendFromISR(queue_, &item, &xHigherPriorityTaskWoken) == pdTRUE;
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking push (returns immediately)
  bool try_pushFromISR(const T& item) {
    BaseType_t xTaskWoken = pdFALSE;
    return xQueueSendFromISR(queue_, &item, &xTaskWoken) == pdTRUE;
  }

  // Blocking pop with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool try_pop_for(T& out, std::chrono::duration<Rep, Period> timeout) {
    return pop(out, timeout);
  }

  // Blocking pop with duration
  template <typename Rep, typename Period>
  bool pop(T& out, std::chrono::duration<Rep, Period> timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
    return pop(out, static_cast<uint32_t>(ms.count()));
  }

  // Blocking pop with timeout in milliseconds
  bool pop(T& out, uint32_t timeout_ms = portMAX_DELAY) {
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue_, &out, ticks) == pdTRUE;
  }

  // Non-blocking pop
  bool try_pop(T& out) { return xQueueReceive(queue_, &out, 0) == pdTRUE; }

  // ISR-safe pop (from ISR context)
  bool popFromISR(T& out) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueReceiveFromISR(queue_, &out, &xHigherPriorityTaskWoken) == pdTRUE;
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking pop (returns immediately)
  bool try_popFromISR(T& out) {
    BaseType_t xTaskWoken = pdFALSE;
    return xQueueReceiveFromISR(queue_, &out, &xTaskWoken) == pdTRUE;
  }

  // Clears all items from the queue
  void clear() { xQueueReset(queue_); }

  size_t size() const { return uxQueueMessagesWaiting(queue_); }

 private:
  QueueHandle_t queue_;
  size_t capacity_;
};

}  // namespace ebus

#endif