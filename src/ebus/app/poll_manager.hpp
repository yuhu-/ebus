/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Manages a registry of recurring eBUS commands.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace ebus {

struct PollItem {
  uint32_t id_;
  uint8_t priority_;
  std::vector<uint8_t> message_;
  std::chrono::seconds interval_;
  std::chrono::steady_clock::time_point next_due_;
  std::function<void(const std::vector<uint8_t>& data)> callback_;
};

class PollManager {
 public:
  PollManager() : next_id_(1) {}

  // Register a new recurring command. Returns a unique ID.
  uint32_t addPollItem(
      uint8_t priority, const std::vector<uint8_t>& message,
      std::chrono::seconds interval,
      std::function<void(const std::vector<uint8_t>&)> cb = nullptr);

  // Remove a recurring command by ID.
  void removePollItem(uint32_t id);

  // Returns a list of commands that are currently due for execution
  // and updates their internal timers.
  std::vector<PollItem> getDueItems();

  void clear();

 private:
  mutable std::mutex mutex_;
  std::vector<PollItem> items_;
  uint32_t next_id_;
};

}  // namespace ebus