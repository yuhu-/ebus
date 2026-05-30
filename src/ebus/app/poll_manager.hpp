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
#include <ebus/status.hpp>
#include <functional>
#include <mutex>
#include <vector>

#include "utils/static_vector.hpp"

namespace ebus::detail {

struct PollItem {
  uint32_t poll_id;
  uint8_t priority;
  Sequence message;
  std::chrono::milliseconds interval;
  Clock::time_point next_due;
  ResultCallback callback;

  struct Greater {
    bool operator()(const PollItem& lhs, const PollItem& rhs) const {
      if (lhs.next_due != rhs.next_due) return lhs.next_due > rhs.next_due;
      return lhs.poll_id > rhs.poll_id;
    }
  };
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

  // Returns the time point when the next item is due, or max if empty.
  Clock::time_point nextDueTime() const;

  void clear();

  PollManagerStatus getStatus() const;

 private:
  mutable std::mutex mutex_;
  StaticVector<PollItem, PollLimits::max_items> items_;
  uint8_t own_address_ = 0xff;
  uint32_t next_poll_id_;
};

}  // namespace ebus::detail