/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>

namespace ebus::detail {

void PollManager::setOwnAddress(uint8_t address) {
  std::lock_guard<std::mutex> lock(mutex_);
  own_address_ = address;
  const uint8_t own_slave = ebus::slaveOf(address);

  auto it = items_.begin();
  while (it != items_.end()) {
    if (!it->message.empty() && it->message[0] == own_slave) {
      it = items_.erase(it);
    } else {
      ++it;
    }
  }
}

uint32_t PollManager::addPollItem(uint8_t priority, ByteView message,
                                  uint32_t interval_ms,
                                  ResultCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Proactively prevent self-polling
  if (!message.empty() && message[0] == ebus::slaveOf(own_address_)) {
    return 0;
  }

  if (items_.size() >= PollLimits::max_items) {
    return 0;
  }

  uint32_t id = next_poll_id_++;
  PollItem item;
  item.poll_id = id;
  item.priority = priority;
  item.message.assign(message);
  item.interval = std::chrono::milliseconds(interval_ms);
  // Schedule immediately to ensure data is available as soon as possible
  item.next_due = std::chrono::steady_clock::now();
  item.callback = std::move(callback);

  items_.insert(std::move(item));
  return id;
}

void PollManager::removePollItem(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [id](const PollItem& i) { return i.poll_id == id; });
  if (it != items_.end()) items_.erase(it);
}

void PollManager::processDueItems(
    const std::function<void(const PollItem&)>& callback, bool* activity) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = std::chrono::steady_clock::now();

  while (!items_.empty() && items_.begin()->next_due <= now) {
    // Extract the due item (node handles are C++17)
    auto node = items_.extract(items_.begin());
    const auto& item = node.value();
    callback(item);
    if (activity) *activity = true;

    // Update timer and re-insert
    node.value().next_due = now + item.interval;
    items_.insert(std::move(node));
  }
}

std::chrono::steady_clock::time_point PollManager::nextDueTime() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (items_.empty())
    return std::chrono::steady_clock::time_point::max();
  return items_.begin()->next_due;
}

void PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}

PollManagerStatus PollManager::getStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  PollManagerStatus s;
  s.item_count = items_.size();
  return s;
}

}  // namespace ebus::detail