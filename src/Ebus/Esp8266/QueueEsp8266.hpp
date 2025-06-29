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

#include <Arduino.h>

namespace ebus {

template <typename T>
class QueueEsp8266 {
 public:
  explicit QueueEsp8266(size_t capacity = 32)
      : m_capacity(capacity), m_size(capacity + 1), m_head(0), m_tail(0) {
    m_buffer = new T[m_size];
  }

  ~QueueEsp8266() { delete[] m_buffer; }

  // Copy constructor
  QueueEsp8266(const QueueEsp8266& other)
      : m_capacity(other.m_capacity),
        m_size(other.m_size),
        m_head(other.m_head),
        m_tail(other.m_tail) {
    m_buffer = new T[m_size];
    for (size_t i = 0; i < m_size; ++i) {
      m_buffer[i] = other.m_buffer[i];
    }
  }

  // Copy assignment operator
  QueueEsp8266& operator=(const QueueEsp8266& other) {
    if (this != &other) {
      delete[] m_buffer;
      m_capacity = other.m_capacity;
      m_size = other.m_size;
      m_head = other.m_head;
      m_tail = other.m_tail;
      m_buffer = new T[m_size];
      for (size_t i = 0; i < m_size; ++i) {
        m_buffer[i] = other.m_buffer[i];
      }
    }
    return *this;
  }

  // Move constructor
  QueueEsp8266(QueueEsp8266&& other) noexcept
      : m_buffer(other.m_buffer),
        m_capacity(other.m_capacity),
        m_size(other.m_size),
        m_head(other.m_head),
        m_tail(other.m_tail) {
    other.m_buffer = nullptr;
    other.m_size = 0;
    other.m_head = 0;
    other.m_tail = 0;
  }

  // Move assignment operator
  QueueEsp8266& operator=(QueueEsp8266&& other) noexcept {
    if (this != &other) {
      delete[] m_buffer;
      m_buffer = other.m_buffer;
      m_capacity = other.m_capacity;
      m_size = other.m_size;
      m_head = other.m_head;
      m_tail = other.m_tail;
      other.m_buffer = nullptr;
      other.m_capacity = 0;
      other.m_size = 0;
      other.m_head = 0;
      other.m_tail = 0;
    }
    return *this;
  }

  // Blocking push with timeout (busy-wait, not recommended for long waits)
  bool push(const T& item, uint32_t timeout_ms = 1000) {
    uint32_t start = millis();
    while (!try_push(item)) {
      if (m_capacity > 0 && size() >= m_capacity) {
        if ((millis() - start) >= timeout_ms) return false;
        yield();
      }
    }
    return true;
  }

  // Non-blocking push (returns false if full)
  bool try_push(const T& item) {
    size_t next = (m_head + 1) % m_size;
    if (m_capacity > 0 && size() >= m_capacity) return false;  // Capacity check
    if (next == m_tail) return false;  // Buffer full (ring buffer)
    m_buffer[m_head] = item;
    m_head = next;
    return true;
  }

  // Blocking pop with timeout (returns false on timeout/empty)
  bool pop(T& out, uint32_t timeout_ms = 1000) {
    uint32_t start = millis();
    while (true) {
      if (try_pop(out)) return true;
      if ((millis() - start) >= timeout_ms) return false;
      yield();
    }
  }

  // Non-blocking pop (returns false if empty)
  bool try_pop(T& out) {
    if (m_head == m_tail) return false;  // Buffer empty
    out = m_buffer[m_tail];
    m_tail = (m_tail + 1) % m_size;
    return true;
  }

  const size_t size() const { return (m_head + m_size - m_tail) % m_size; }

  const size_t capacity() const { return m_capacity; }

 private:
  T* m_buffer;
  size_t m_capacity;  // store user-requested capacity
  size_t m_size, m_head, m_tail;
};

}  // namespace ebus
