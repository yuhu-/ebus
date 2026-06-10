/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <mutex>
#endif

#include <condition_variable>
#include <ebus/types.hpp>

namespace ebus::detail::platform {

/**
 * @brief Platform-agnostic Non-Recursive Mutex.
 * Uses raw FreeRTOS semaphores on ESP32 to bypass the pthread layer.
 */
class Mutex {
 public:
  Mutex() {
#if defined(ESP_PLATFORM)
    handle_ = xSemaphoreCreateMutex();
#endif
  }

  ~Mutex() {
#if defined(ESP_PLATFORM)
    if (handle_) vSemaphoreDelete(handle_);
#endif
  }

  void lock() {
#if defined(ESP_PLATFORM)
    xSemaphoreTake(handle_, portMAX_DELAY);
#else
    mutex_.lock();
#endif
  }

  void unlock() {
#if defined(ESP_PLATFORM)
    xSemaphoreGive(handle_);
#else
    mutex_.unlock();
#endif
  }

  bool try_lock() {
#if defined(ESP_PLATFORM)
    return xSemaphoreTake(handle_, 0) == pdTRUE;
#else
    return mutex_.try_lock();
#endif
  }

  // Special Members
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

 private:
#if defined(ESP_PLATFORM)
  SemaphoreHandle_t handle_;
#else
  std::mutex mutex_;
#endif
};

/**
 * @brief RAII guard for the platform Mutex.
 * Compatible with std::lock_guard logic.
 */
template <typename M>
using LockGuard = std::lock_guard<M>;

/**
 * @brief RAII guard for the platform Mutex.
 * Compatible with std::unique_lock logic.
 */
template <typename M>
using UniqueLock = std::unique_lock<M>;

/**
 * @brief Platform-agnostic Recursive Mutex.
 * Uses raw FreeRTOS recursive semaphores on ESP32 to bypass the pthread layer.
 */
class RecursiveMutex {
 public:
  RecursiveMutex() {
#if defined(ESP_PLATFORM)
    handle_ = xSemaphoreCreateRecursiveMutex();
#endif
  }

  ~RecursiveMutex() {
#if defined(ESP_PLATFORM)
    if (handle_) vSemaphoreDelete(handle_);
#endif
  }

  void lock() {
#if defined(ESP_PLATFORM)
    xSemaphoreTakeRecursive(handle_, portMAX_DELAY);
#else
    mutex_.lock();
#endif
  }

  void unlock() {
#if defined(ESP_PLATFORM)
    xSemaphoreGiveRecursive(handle_);
#else
    mutex_.unlock();
#endif
  }

  bool try_lock() {
#if defined(ESP_PLATFORM)
    return xSemaphoreTakeRecursive(handle_, 0) == pdTRUE;
#else
    return mutex_.try_lock();
#endif
  }

  // Special Members
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;

 private:
#if defined(ESP_PLATFORM)
  SemaphoreHandle_t handle_;
#else
  std::recursive_mutex mutex_;
#endif
};

/**
 * @brief Platform-agnostic Condition Variable.
 * Uses FreeRTOS Event Groups on ESP32 and std::condition_variable on POSIX.
 */
class ConditionVariable {
 public:
  ConditionVariable() {
#if defined(ESP_PLATFORM)
    event_group_ = xEventGroupCreate();
#endif
  }

  ~ConditionVariable() {
#if defined(ESP_PLATFORM)
    if (event_group_) vEventGroupDelete(event_group_);
#endif
  }

  template <class MutexType, class Predicate>
  void wait(UniqueLock<MutexType>& lock, Predicate pred) {
#if defined(ESP_PLATFORM)
    while (!pred()) {
      lock.unlock();
      xEventGroupWaitBits(event_group_,
                          0x01,            // Wait for bit 0
                          pdTRUE,          // Clear bit on exit
                          pdFALSE,         // Don't wait for all bits
                          portMAX_DELAY);  // Wait indefinitely
      lock.lock();
    }
#else
    cv_.wait(lock, pred);
#endif
  }

  template <class MutexType, class Clock, class Duration>
  std::cv_status wait_until(
      UniqueLock<MutexType>& lock,
      const std::chrono::time_point<Clock, Duration>& timeout_time) {
#if defined(ESP_PLATFORM)
    auto now = Clock::now();
    if (timeout_time <= now) return std::cv_status::timeout;
    auto remaining_duration = timeout_time - now;
    TickType_t ticks_to_wait =
        pdMS_TO_TICKS(std::chrono::duration_cast<std::chrono::milliseconds>(
                          remaining_duration)
                          .count());
    if (ticks_to_wait == 0 && remaining_duration > std::chrono::milliseconds(0))
      ticks_to_wait = 1;

    lock.unlock();
    EventBits_t bits =
        xEventGroupWaitBits(event_group_, 0x01, pdTRUE, pdFALSE, ticks_to_wait);
    lock.lock();
    return (bits & 0x01) ? std::cv_status::no_timeout : std::cv_status::timeout;
#else
    return cv_.wait_until(lock, timeout_time);
#endif
  }

  template <class MutexType, class Clock, class Duration, class Predicate>
  bool wait_until(UniqueLock<MutexType>& lock,
                  const std::chrono::time_point<Clock, Duration>& timeout_time,
                  Predicate pred) {
#if defined(ESP_PLATFORM)
    auto now = Clock::now();
    if (timeout_time <= now) {
      return pred();  // Check predicate immediately if timeout already passed
    }
    auto remaining_duration = timeout_time - now;
    TickType_t ticks_to_wait =
        pdMS_TO_TICKS(std::chrono::duration_cast<std::chrono::milliseconds>(
                          remaining_duration)
                          .count());
    if (ticks_to_wait == 0 &&
        remaining_duration > std::chrono::milliseconds(0)) {
      ticks_to_wait = 1;  // Ensure at least one tick if duration is non-zero
                          // but less than 1ms
    }

    while (!pred()) {
      lock.unlock();
      EventBits_t bits =
          xEventGroupWaitBits(event_group_,
                              0x01,            // Wait for bit 0
                              pdTRUE,          // Clear bit on exit
                              pdFALSE,         // Don't wait for all bits
                              ticks_to_wait);  // Wait with timeout
      lock.lock();
      if (!(bits & 0x01) &&
          !pred()) {  // If timed out and predicate still false
        return false;
      }
    }
    return true;
#else
    return cv_.wait_until(lock, timeout_time, pred);
#endif
  }

  template <class MutexType, class Rep, class Period>
  std::cv_status wait_for(UniqueLock<MutexType>& lock,
                          const std::chrono::duration<Rep, Period>& rel_time) {
    return wait_until(lock, Clock::now() + rel_time);
  }

  template <class MutexType, class Rep, class Period, class Predicate>
  bool wait_for(UniqueLock<MutexType>& lock,
                const std::chrono::duration<Rep, Period>& rel_time,
                Predicate pred) {
    return wait_until(lock, Clock::now() + rel_time, pred);
  }

  void notify_one() {
#if defined(ESP_PLATFORM)
    xEventGroupSetBits(event_group_, 0x01);  // Set bit 0
#else
    cv_.notify_one();
#endif
  }

  void notify_all() {
#if defined(ESP_PLATFORM)
    xEventGroupSetBits(event_group_, 0x01);
#else
    cv_.notify_all();
#endif
  }

 private:
#if defined(ESP_PLATFORM)
  EventGroupHandle_t event_group_;
#else
  std::condition_variable_any cv_;
#endif
};

}  // namespace ebus::detail::platform
