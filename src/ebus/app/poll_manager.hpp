/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Manages a registry of recurring eBUS commands.

#pragma once

#include <chrono>
#include <cstdint>
#include <ebus/callbacks.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <functional>
#include <mutex>
#include <set>
#include <vector>

namespace ebus::detail {

struct PollItem {
  uint32_t poll_id;
  uint8_t priority;
  Sequence message;
  std::chrono::milliseconds interval;
  std::chrono::steady_clock::time_point next_due;
  ResultCallback callback;

  bool operator<(const PollItem& other) const {
    if (next_due != other.next_due) return next_due < other.next_due;
    return poll_id < other.poll_id;
  }
};

/**
 * The PollManager allows registering recurring eBUS commands with specified
 * intervals and priorities. It maintains an internal schedule and provides a
 * method to process due commands, which can be called from the main loop or a
 * timer. The manager ensures thread safety for concurrent access and allows
 * dynamic addition and removal of poll items. Each item can have an optional
 * callback that is invoked when the command is processed, allowing for flexible
 * handling of responses or side effects.
 */
class PollManager {
 public:
  PollManager() : next_poll_id_(1) {}

  // Sets the current master address and purges items that would poll itself.
  void setOwnAddress(uint8_t address);

  // Register a new recurring command. Returns a unique ID.
  uint32_t addPollItem(uint8_t priority, ByteView message, uint32_t interval_ms,
                       ResultCallback callback = nullptr);

  // Remove a recurring command by ID.
  void removePollItem(uint32_t id);

  // Processes commands that are currently due and updates their internal
  // timers. Using a callback avoids heap allocations from returning a vector.
  void processDueItems(const std::function<void(const PollItem&)>& callback,
                       bool* activity);

  void clear();

  PollStatus getStatus() const;

 private:
  mutable std::mutex mutex_;
  std::set<PollItem> items_;
  uint32_t next_poll_id_;
};

}  // namespace ebus::detail