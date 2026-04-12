/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>

uint32_t ebus::PollManager::addPollItem(
    uint8_t priority, const std::vector<uint8_t>& message,
    std::chrono::seconds interval,
    std::function<void(const std::vector<uint8_t>&)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t id = next_id_++;
  PollItem item{id,
                priority,
                message,
                interval,
                std::chrono::steady_clock::now() + interval,
                std::move(callback)};
  items_.push_back(std::move(item));
  return id;
}

void ebus::PollManager::removePollItem(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.erase(std::remove_if(items_.begin(), items_.end(),
                              [id](const PollItem& i) { return i.id == id; }),
               items_.end());
}

std::vector<ebus::PollItem> ebus::PollManager::getDueItems() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<PollItem> due;
  auto now = std::chrono::steady_clock::now();

  for (auto& item : items_) {
    if (now >= item.next_due) {
      due.push_back(item);
      // Schedule next occurrence based on current "now" to avoid
      // drift or accumulation if the bus was busy.
      item.next_due = now + item.interval;
    }
  }
  return due;
}

void ebus::PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}