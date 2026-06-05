/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <chrono>

namespace ebus::detail::platform {

template <typename T>
class QueueEsp {
 public:
  explicit QueueEsp(size_t capacity = 32)
      : queue_(xQueueCreate(capacity, sizeof(T))),
        capacity_(capacity),
        shutdown_(false) {}

  ~QueueEsp() {
    if (queue_) vQueueDelete(queue_);
  }

  // Blocking push with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPushFor(const T& item, std::chrono::duration<Rep, Period> timeout) {
    return push(item, timeout);
  }

  // Blocking push (move semantics) with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPushFor(T&& item, std::chrono::duration<Rep, Period> timeout) {
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
    if (shutdown_) return false;
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue_, &item, ticks) == pdTRUE;
  }

  // Blocking push (move semantics) with timeout in milliseconds
  bool push(T&& item, uint32_t timeout_ms = portMAX_DELAY) {
    if (shutdown_) return false;
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue_, &item, ticks) == pdTRUE;
  }

  // Non-blocking push
  bool tryPush(const T& item) {
    if (shutdown_) return false;
    return xQueueSend(queue_, &item, 0) == pdTRUE;
  }

  // Non-blocking push (move semantics)
  bool tryPush(T&& item) {
    if (shutdown_) return false;
    return xQueueSend(queue_, &item, 0) == pdTRUE;
  }

  // ISR-safe push (from ISR context)
  bool pushFromISR(const T& item) {
    if (shutdown_) return false;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueSendFromISR(queue_, &item, &xHigherPriorityTaskWoken) == pdTRUE;
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking push (returns immediately)
  bool tryPushFromISR(const T& item) {
    if (shutdown_) return false;
    BaseType_t xTaskWoken = pdFALSE;
    return xQueueSendFromISR(queue_, &item, &xTaskWoken) == pdTRUE;
  }

  // Blocking pop with duration (Standard C++ naming alias)
  template <typename Rep, typename Period>
  bool tryPopFor(T& out, std::chrono::duration<Rep, Period> timeout) {
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
    if (shutdown_ && uxQueueMessagesWaiting(queue_) == 0) return false;
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue_, &out, ticks) == pdTRUE;
  }

  // Non-blocking pop
  bool tryPop(T& out) {
    if (shutdown_ && uxQueueMessagesWaiting(queue_) == 0) return false;
    return xQueueReceive(queue_, &out, 0) == pdTRUE;
  }

  // ISR-safe pop (from ISR context)
  bool popFromISR(T& out) {
    if (shutdown_ && uxQueueMessagesWaiting(queue_) == 0) return false;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueReceiveFromISR(queue_, &out, &xHigherPriorityTaskWoken) == pdTRUE;
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking pop (returns immediately)
  bool tryPopFromISR(T& out) {
    if (shutdown_ && uxQueueMessagesWaiting(queue_) == 0) return false;
    BaseType_t xTaskWoken = pdFALSE;
    return xQueueReceiveFromISR(queue_, &out, &xTaskWoken) == pdTRUE;
  }

  // Clears all items from the queue
  void clear() { xQueueReset(queue_); }

  void reset() {
    xQueueReset(queue_);
    shutdown_.store(false);
  }

  void shutdown() { shutdown_.store(true); }
  bool isShutdown() const { return shutdown_.load(); }

  size_t size() const { return uxQueueMessagesWaiting(queue_); }

 private:
  QueueHandle_t queue_;
  size_t capacity_;
  std::atomic<bool> shutdown_;
};

}  // namespace ebus::detail::platform

#endif  // ESP_PLATFORM