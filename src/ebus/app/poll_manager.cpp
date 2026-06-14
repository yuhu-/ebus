/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/poll_manager.hpp"

#include <algorithm>
#include <ebus/detail/json_reader.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

PollManager::PollManager() : next_poll_id_(1) {}

void PollManager::setOwnAddress(uint8_t address) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
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

void PollManager::setBusyPredicate(Delegate<bool()> pred) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  is_busy_ = std::move(pred);
}

uint32_t PollManager::addPollItem(uint8_t priority, ByteView message,
                                  uint32_t interval_ms) {
  platform::LockGuard<platform::Mutex> lock(mutex_);

  // Safety: Prevent infinite loops with 0ms intervals
  if (interval_ms == 0) interval_ms = 1;

  // Proactively prevent self-polling
  if (!message.empty() && message[0] == ebus::slaveOf(own_address_)) {
    return 0;
  }

  if (items_.size() >= ebus::RuntimeConfig{}.poll.max_items) {
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

  items_.push_back(std::move(item));
  if (items_.size() > max_item_count_) {
    max_item_count_ = items_.size();
  }
  std::push_heap(items_.begin(), items_.end(), Item::Greater());
  return id;
}

void PollManager::removePollItem(uint32_t id) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  auto it = std::find_if(items_.begin(), items_.end(),
                         [id](const Item& i) { return i.poll_id == id; });
  if (it != items_.end()) items_.erase(it);
}

void PollManager::processDueItems(PollVisitor callback, bool* activity) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  auto now = Clock::now();

  // Safety: If the system is busy (e.g. Scheduler full), do not process
  // items. This prevents "skipping" polls when the scheduler is momentarily
  // unable to accept new messages. The items will remain in the heap and
  // nextDueTime() will continue to indicate that work is pending.
  while (!items_.empty() && items_.front().next_due <= now &&
         (!is_busy_ || !is_busy_())) {
    std::pop_heap(items_.begin(), items_.end(), Item::Greater());
    Item item = std::move(items_.back());
    items_.pop_back();

    callback(item);  // Process item
    if (activity) *activity = true;

    // Reschedule
    item.next_due = now + item.interval;
    // Safety check for clock jitter or 0 intervals
    if (item.next_due <= now)
      item.next_due = now + std::chrono::milliseconds(1);

    items_.push_back(std::move(item));
    std::push_heap(items_.begin(), items_.end(), Item::Greater());
  }
}

bool PollManager::mergeFromJson(const std::string& json) {
  detail::JsonReader reader(json);
  if (reader.next() != detail::JsonReader::Token::array_start) return false;

  while (true) {
    auto t = reader.next();
    if (t == detail::JsonReader::Token::array_end ||
        t == detail::JsonReader::Token::end)
      break;
    if (t == detail::JsonReader::Token::error) return false;

    if (t == detail::JsonReader::Token::object_start) {
      uint8_t priority = 5;
      std::string message_hex;
      uint32_t interval = 1000;
      bool ok = true;

      reader.forEachField([&](std::string_view key, detail::JsonReader& inner) {
        if (key == "priority") {
          inner.next();
          auto val = inner.asNumStrict<int>();
          if (val)
            priority = static_cast<uint8_t>(*val);
          else
            ok = false;
          return val.has_value();
        } else if (key == "message") {
          inner.next();
          message_hex = inner.value();
          return true;
        } else if (key == "interval_ms") {
          inner.next();
          auto val = inner.asNumStrict<uint32_t>();
          if (val)
            interval = *val;
          else
            ok = false;
          return val.has_value();
        }
        return false;
      });

      if (ok && !message_hex.empty()) {
        addPollItem(priority, ebus::toVector(message_hex), interval);
      }
    } else {
      reader.skipValue();
    }
  }
  return true;
}

void PollManager::clear() {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  items_.clear();
}

Clock::time_point PollManager::nextDueTime() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);

  // Starvation Fix: If the system is busy, the PollManager should not
  // request a wakeup "now", as it would lead to a tight spin loop in the
  // controller.
  if (is_busy_ && is_busy_()) {
    return Clock::time_point::max();
  }

  if (items_.empty()) return Clock::time_point::max();
  return items_.front().next_due;
}

void PollManager::resetPeakMetrics() {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  max_item_count_ = items_.size();
}

PollManagerStatus PollManager::fetchStatus() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  return PollManagerStatus{items_.size(), max_item_count_,
                           ebus::RuntimeConfig{}.poll.max_items};
}

}  // namespace ebus::detail