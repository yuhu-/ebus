/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>

uint32_t ebus::PollManager::addPollItem(
    uint8_t priority, ByteView message, std::chrono::seconds interval,
    std::function<void(ByteView)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t id = next_id_++;
  PollItem item;
  item.id = id;
  item.priority = priority;
  item.message.assign(message);
  item.interval = interval;
  item.next_due = std::chrono::steady_clock::now() + interval;
  item.callback = std::move(callback);

  items_.insert(std::move(item));
  return id;
}

void ebus::PollManager::removePollItem(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [id](const PollItem& i) { return i.id == id; });
  if (it != items_.end()) items_.erase(it);
}

void ebus::PollManager::processDueItems(
    const std::function<void(const PollItem&)>& callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = std::chrono::steady_clock::now();

  while (!items_.empty() && now >= items_.begin()->next_due) {
    // Extract the due item (node handles are C++17)
    auto node = items_.extract(items_.begin());
    callback(node.value());

    // Update timer and re-insert
    node.value().next_due = now + node.value().interval;
    items_.insert(std::move(node));
  }
}

void ebus::PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}