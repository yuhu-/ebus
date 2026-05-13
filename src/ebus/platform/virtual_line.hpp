/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>

namespace ebus::detail::platform {

/**
 * Thread-safe virtual eBUS wire with broadcast support.
 * Acts as a singleton "wire" allowing multiple Bus instances to communicate.
 */
class VirtualLine {
 public:
  static VirtualLine& get() {
    static VirtualLine instance;
    return instance;
  }

  inline void attach(void* bus_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    queues_[bus_key] = std::queue<uint8_t>();
  }

  inline void detach(void* bus_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    queues_.erase(bus_key);
  }

  inline void write(uint8_t byte) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto& pair : queues_) {
        pair.second.push(byte);
      }
    }
    cv_.notify_all();
  }

  inline bool read(void* bus_key, uint8_t& out, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = queues_.find(bus_key);
    if (it == queues_.end()) return false;

    auto& q = it->second;
    auto tp = Clock::now() + std::chrono::milliseconds(timeout_ms);

    if (!cv_.wait_until(lock, tp,
                        [this, &q] { return !q.empty() || shutdown_; })) {
      return false;
    }

    if (shutdown_ || q.empty()) return false;
    out = q.front();
    q.pop();
    return true;
  }

  inline void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
    }
    cv_.notify_all();
  }

  inline void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : queues_) {
      std::queue<uint8_t> empty;
      std::swap(pair.second, empty);
    }
  }

 private:
  VirtualLine() = default;
  std::map<void*, std::queue<uint8_t>> queues_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool shutdown_ = false;
};

}  // namespace ebus::detail::platform
