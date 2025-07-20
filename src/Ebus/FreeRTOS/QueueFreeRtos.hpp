/*
 * Copyright (C) 2025 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace ebus {

template <typename T>
class QueueFreeRtos {
 public:
  explicit QueueFreeRtos(size_t capacity = 32) : m_capacity(capacity) {
    queue = xQueueCreate(capacity, sizeof(T));
  }

  ~QueueFreeRtos() {
    if (queue) vQueueDelete(queue);
  }

  // Blocking push with timeout (timeout in milliseconds)
  bool push(const T& item, uint32_t timeout_ms = portMAX_DELAY) {
    if (m_capacity > 0 && size() >= m_capacity) return false;  // Capacity check
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue, &item, ticks) == pdTRUE;
  }

  // Non-blocking push
  bool try_push(const T& item) {
    if (m_capacity > 0 && size() >= m_capacity) return false;  // Capacity check
    return xQueueSend(queue, &item, 0) == pdTRUE;
  }

  // ISR-safe push (from ISR context)
  bool pushFromISR(const T& item) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueSendFromISR(queue, &item, &xHigherPriorityTaskWoken) == pdTRUE;
    // Optionally yield if a higher priority task was woken
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking push (returns immediately)
  bool try_pushFromISR(const T& item) {
    BaseType_t xTaskWoken = pdFALSE;
    // 0 timeout means non-blocking
    bool result = xQueueSendFromISR(queue, &item, &xTaskWoken) == pdTRUE;
    // Do NOT yield here for try variant
    return result;
  }

  // Blocking pop with timeout (timeout in milliseconds)
  bool pop(T& out, uint32_t timeout_ms = portMAX_DELAY) {
    if (size() == 0) return false;  // Empty check
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue, &out, ticks) == pdTRUE;
  }

  // Non-blocking pop
  bool try_pop(T& out) {
    if (size() == 0) return false;  // Empty check
    return xQueueReceive(queue, &out, 0) == pdTRUE;
  }

  // ISR-safe pop (from ISR context)
  bool popFromISR(T& out) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool result =
        xQueueReceiveFromISR(queue, &out, &xHigherPriorityTaskWoken) == pdTRUE;
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return result;
  }

  // ISR-safe, non-blocking pop (returns immediately)
  bool try_popFromISR(T& out) {
    BaseType_t xTaskWoken = pdFALSE;
    bool result = xQueueReceiveFromISR(queue, &out, &xTaskWoken) == pdTRUE;
    // Do NOT yield here for try variant
    return result;
  }

  const size_t size() const { return uxQueueMessagesWaiting(queue); }

  const size_t capacity() const { return m_capacity; }

 private:
  QueueHandle_t queue;
  size_t m_capacity;
};

}  // namespace ebus
