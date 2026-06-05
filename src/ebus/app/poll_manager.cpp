/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>

#include "utils/logger.hpp"

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
  Item item;
  item.poll_id = id;
  item.priority = priority;
  item.message.assign(message);
  item.interval = std::chrono::milliseconds(interval_ms);
  // Schedule immediately to ensure data is available as soon as possible
  item.next_due = Clock::now();
  item.callback = std::move(callback);

  items_.push_back(std::move(item));
  if (items_.size() > max_item_count_) {
    max_item_count_ = items_.size();
  }
  std::push_heap(items_.begin(), items_.end(), Item::Greater());
  return id;
}

void PollManager::removePollItem(uint32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [id](const Item& i) { return i.poll_id == id; });
  if (it != items_.end()) items_.erase(it);
}

void PollManager::processDueItems(
    const std::function<void(const Item&)>& callback, bool* activity) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = Clock::now();

  while (!items_.empty() && items_.front().next_due <= now) {
    std::pop_heap(items_.begin(), items_.end(), Item::Greater());
    Item item = std::move(items_.back());
    items_.pop_back();

    EBUS_LOG_DEBUG(
        "[PollManager][0x" + ebus::toString(own_address_) + "] Item " +
        std::to_string(item.poll_id) +
        " is due. Interval=" + std::to_string(item.interval.count()) + "ms");

    callback(item);  // Process item
    if (activity) *activity = true;

    // Reschedule
    item.next_due = now + item.interval;
    items_.push_back(std::move(item));
    std::push_heap(items_.begin(), items_.end(), Item::Greater());
  }
}

Clock::time_point PollManager::nextDueTime() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (items_.empty()) return Clock::time_point::max();
  return items_.front().next_due;
}

void PollManager::resetPeakMetrics() {
  std::lock_guard<std::mutex> lock(mutex_);
  max_item_count_ = items_.size();
}

void PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}

PollManagerStatus PollManager::getStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return PollManagerStatus{items_.size(), max_item_count_};
}

}  // namespace ebus::detail