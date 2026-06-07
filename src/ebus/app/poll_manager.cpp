/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/detail/json_reader.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

PollManager::PollManager() : next_poll_id_(1) {}

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

    callback(item);  // Process item
    if (activity) *activity = true;

    // Reschedule
    item.next_due = now + item.interval;
    items_.push_back(std::move(item));
    std::push_heap(items_.begin(), items_.end(), Item::Greater());
  }
}

bool PollManager::mergeFromJson(const std::string& json) {
  detail::JsonReader reader(json);
  if (reader.next() != detail::JsonReader::Token::ArrayStart) return false;

  while (true) {
    auto t = reader.next();
    if (t == detail::JsonReader::Token::ArrayEnd ||
        t == detail::JsonReader::Token::End)
      break;
    if (t == detail::JsonReader::Token::Error) return false;

    if (t == detail::JsonReader::Token::ObjectStart) {
      uint8_t priority = 5;
      std::string message_hex;
      uint32_t interval = 1000;

      reader.forEachField([&](std::string_view key, detail::JsonReader& inner) {
        if (key == "priority") {
          inner.next();
          priority = static_cast<uint8_t>(inner.asNum<int>());
          return true;
        } else if (key == "message") {
          inner.next();
          message_hex = inner.value();
          return true;
        } else if (key == "interval_ms") {
          inner.next();
          interval = inner.asNum<uint32_t>();
          return true;
        }
        return false;
      });

      if (!message_hex.empty()) {
        addPollItem(priority, ebus::toVector(message_hex), interval);
      }
    } else {
      reader.skipValue();
    }
  }
  return true;
}

void PollManager::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
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

PollManagerStatus PollManager::getStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return PollManagerStatus{items_.size(), max_item_count_};
}

}  // namespace ebus::detail