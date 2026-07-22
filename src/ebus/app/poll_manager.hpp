/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Manages a registry of recurring eBUS commands.

#pragma once

#include <chrono>
#include <cstdint>
#include <ebus/config.hpp>
#include <ebus/detail/delegate.hpp>
#include <ebus/sequence.hpp>
#include <ebus/static_vector.hpp>
#include <ebus/status.hpp>

#include "platform/mutex.hpp"

namespace ebus::detail {

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
  // --- Public Types & Constants ---
  using PollSequence = SequenceImpl<detail::SequenceLimits::poll_capacity>;

  struct Item {
    uint32_t poll_id;
    uint8_t priority;
    PollSequence message;
    std::chrono::milliseconds interval;
    Clock::time_point next_due;

    struct Greater {
      bool operator()(const Item& lhs, const Item& rhs) const {
        if (lhs.next_due != rhs.next_due) return lhs.next_due > rhs.next_due;
        return lhs.poll_id > rhs.poll_id;
      }
    };
  };

  // Lifecycle & Static Factories
  PollManager();

  // Special Members & Operators
  PollManager(const PollManager&) = delete;
  PollManager& operator=(const PollManager&) = delete;

  // Configuration
  // Sets the current master address and purges items that would poll itself.
  void setOwnAddress(uint8_t address);

  // Sets a predicate to check if the system is too busy to process polls.
  // Prevents unnecessary scheduling attempts when the Scheduler is full.
  void setBusyPredicate(Delegate<bool()> pred);

  // Working Methods
  // Register a new recurring command. Returns a unique ID.
  uint32_t addPollItem(uint8_t priority, ByteView message,
                       uint32_t interval_ms);
  // Remove a recurring command by ID.
  void removePollItem(uint32_t id);

  // Processes commands that are currently due and updates their internal
  // timers. Using a callback avoids heap allocations from returning a vector.
  void processDueItems(Delegate<void(const Item&)> callback, bool* activity);

  /**
   * @brief Deserializes and adds poll items from a JSON array.
   * Expected format: [{"priority": 10, "message": "aabbcc", "interval_ms":
   * 1000}, ...]
   * @return true if the JSON was partially or fully parsed.
   */
  bool mergeFromJson(const std::string& json);

  void clear();

  // Status/Telemetry
  // Returns the time point when the next item is due, or max if empty.
  Clock::time_point nextDueTime() const;
  void resetPeakMetrics();
  PollManagerStatus fetchStatus() const;

 private:
  mutable platform::Mutex mutex_;
  StaticVector<Item, ebus::RuntimeConfig{}.poll.max_items> items_;
  uint8_t own_address_ = 0xff;
  Delegate<bool()> is_busy_;
  size_t max_item_count_ = 0;
  uint32_t next_poll_id_;
};

}  // namespace ebus::detail