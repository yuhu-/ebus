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

  const size_t size() { return uxQueueMessagesWaiting(queue); }

  const size_t capacity() { return m_capacity; }

 private:
  QueueHandle_t queue;
  size_t m_capacity;
};

}  // namespace ebus
