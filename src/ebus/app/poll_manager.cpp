/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>

namespace ebus::detail {

uint32_t PollManager::addPollItem(uint8_t priority, ByteView message,
                                  std::chrono::milliseconds interval,
                                  ResultCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t id = next_id_++;
  PollItem item;
  item.id = id;
  item.priority = priority;
  item.message.assign(message);
  item.interval = interval;
  // Schedule immediately to ensure data is available as soon as possible
  item.next_due = std::chrono::steady_clock::now();
  item.callback = std::move(callback);

  items_.insert(std::move(item));
  return id;
}

void PollManager::removePollItem(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [id](const PollItem& i) { return i.id == id; });
  if (it != items_.end()) items_.erase(it);
}

void PollManager::processDueItems(
    const std::function<void(const PollItem&)>& callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = std::chrono::steady_clock::now();

  while (!items_.empty() && items_.begin()->next_due <= now) {
    // Extract the due item (node handles are C++17)
    auto node = items_.extract(items_.begin());
    const auto& item = node.value();
    callback(item);

    // Update timer and re-insert
    node.value().next_due = now + item.interval;
    items_.insert(std::move(node));
  }
}

void PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}

}  // namespace ebus::detail