/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/scheduler.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ebus/utils.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

ebus::Scheduler::Scheduler(Handler* handler)
    : handler_(handler),
      stop_flag_(true),
      next_id_(1),
      max_send_attempts_(3),
      base_backoff_(std::chrono::milliseconds(100)) {
  attachHandlerCallbacks();
}

ebus::Scheduler::~Scheduler() { stop(); }

void ebus::Scheduler::start() {
  bool expected = true;
  if (stop_flag_.compare_exchange_strong(expected, false)) {
    worker_ = std::make_unique<ServiceThread>(
        "ebusScheduler", [this] { run(); }, 4096, 5, 1);
    worker_->start();
  }
}

void ebus::Scheduler::stop() {
  bool expected = false;
  if (stop_flag_.compare_exchange_strong(expected, true)) {
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      data_ready_cv_.notify_all();
    }

    detachHandlerCallbacks();

    if (worker_) worker_->join();
  }
}

void ebus::Scheduler::enqueue(uint8_t priority, ByteView message,
                              ResultCallback callback) {
  Item it;
  it.priority = priority;
  it.due = Clock::now();
  it.message.assign(message);
  it.result_callback = std::move(callback);
  it.id = next_id_++;
  pushItem(std::move(it));
}

void ebus::Scheduler::enqueueAt(uint8_t priority, ByteView message,
                                TimePoint when, ResultCallback callback) {
  Item it;
  it.priority = priority;
  it.due = when;
  it.message.assign(message);
  it.result_callback = std::move(callback);
  it.id = next_id_++;
  pushItem(std::move(it));
}

void ebus::Scheduler::setMaxSendAttempts(int send_attempts) {
  max_send_attempts_ = std::max(1, send_attempts);
}

void ebus::Scheduler::setBaseBackoff(Duration duration) {
  base_backoff_ = duration;
}

void ebus::Scheduler::setTelegramCallback(TelegramCallback callback) {
  extern_telegram_callback_ = std::move(callback);
}

void ebus::Scheduler::setErrorCallback(ErrorCallback callback) {
  extern_error_callback_ = std::move(callback);
}

size_t ebus::Scheduler::queueSize() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return item_queue_.size();
}

void ebus::Scheduler::clear() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  item_queue_.clear();
  std::make_heap(item_queue_.begin(), item_queue_.end(), Compare());
}

void ebus::Scheduler::pushItem(Item&& it) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  item_queue_.push_back(std::move(it));
  std::push_heap(item_queue_.begin(), item_queue_.end(), Compare());
  data_ready_cv_.notify_one();
}

void ebus::Scheduler::run() {
  std::unique_lock<std::mutex> lock(data_mutex_);

  // Main loop: wait for next due item, attempt to send, and handle retries if
  // needed
  while (!stop_flag_.load()) {
    if (item_queue_.empty()) {
      data_ready_cv_.wait(
          lock, [this] { return stop_flag_.load() || !item_queue_.empty(); });
      if (stop_flag_.load()) break;
    }

    // copy next due while holding lock
    auto next_due = item_queue_.front().due;

    data_ready_cv_.wait_until(lock, next_due, [this, next_due] {
      return stop_flag_.load() || item_queue_.empty() ||
             item_queue_.front().due <= Clock::now() ||
             item_queue_.front().due < next_due;
    });

    if (stop_flag_.load()) break;
    if (item_queue_.empty()) continue;
    if (item_queue_.front().due > Clock::now()) continue;

    std::pop_heap(item_queue_.begin(), item_queue_.end(), Compare());
    Item current_item = std::move(item_queue_.back());
    item_queue_.pop_back();

    lock.unlock();

    bool sent = false;
    std::string last_error = "unknown";
    Sequence slave_response;
    uint32_t attempt_id = current_item.id;

    current_attempt_id_.store(attempt_id, std::memory_order_relaxed);

    // Clear any stale events from previous attempts
    Event stale;
    while (event_queue_.tryPop(stale));

    if (stop_flag_.load()) break;

    // Initiation: check if handler is busy, then arm it.
    if (handler_->isActiveMessagePending()) {
      last_error = "Handler busy";
    } else if (!handler_->sendActiveMessage(current_item.message)) {
      last_error = "Invalid message";
    }

    // If arming succeeded, wait for terminal events
    if (last_error == "unknown") {
      auto start = Clock::now();
      while (!stop_flag_.load()) {
        Event ev;
        if (!event_queue_.pop(ev, 2000)) {
          last_error = "FSM timeout";
          handler_->reset();  // Serious error: FSM stuck.
          break;
        }

        if (ev.id != attempt_id) continue;

        if (ev.type == EventType::telegram) {
          sent = true;
          slave_response = std::move(ev.slave);
          break;
        } else if (ev.type == EventType::lost) {
          last_error = "Arbitration lost";
          break;
        } else if (ev.type == EventType::error) {
          last_error = ev.error;
          break;
        }

        if (Clock::now() - start > std::chrono::seconds(4)) {
          last_error = "Total transfer timeout";
          handler_->reset();
          break;
        }
      }
    }

    // clear attempt id so stray callbacks are ignored
    current_attempt_id_.store(0, std::memory_order_relaxed);

    if (sent) {
      if (current_item.result_callback)
        current_item.result_callback(true, current_item.message,
                                     slave_response);
      lock.lock();
    } else {
      ++current_item.send_attempts;
      if (current_item.send_attempts < max_send_attempts_) {
        current_item.due =
            Clock::now() + backoffDuration(current_item.send_attempts);
        lock.lock();
        item_queue_.push_back(std::move(current_item));
        std::push_heap(item_queue_.begin(), item_queue_.end(), Compare());
        data_ready_cv_.notify_one();
      } else {
        if (extern_error_callback_) {
          extern_error_callback_(last_error, current_item.message, {});
        }
        if (current_item.result_callback) {
          current_item.result_callback(false, current_item.message, {});
        }
        lock.lock();
      }
    }
  }
}

ebus::Scheduler::Duration ebus::Scheduler::backoffDuration(int attempt) const {
  // exponential backoff: base * 2^(attempt-1)
  int shift = std::max(0, attempt - 1);
  // cap shift to avoid undefined/overflowed shifts
  constexpr int kMaxShift = 30;
  shift = std::min(shift, kMaxShift);

  // multiply base_backoff_ by (1 << shift) using integer multiplication on
  // count() to avoid accidental scaling issues with some duration types.
  using Rep = typename Duration::rep;

  // Cap shift to prevent overflow if Rep is small or base is large
  if (shift >= static_cast<int>(sizeof(Rep) * 8 - 2)) return Duration::max();

  Rep factor = static_cast<Rep>(1) << shift;
  return Duration(static_cast<Rep>(base_backoff_.count() * factor));
}

void ebus::Scheduler::attachHandlerCallbacks() {
  if (!handler_) return;

  handler_->setBusRequestWonCallback([this]() {
    uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::won;
    ev.id = id;
    event_queue_.tryPush(ev);
  });

  handler_->setBusRequestLostCallback([this]() {
    uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::lost;
    ev.id = id;
    event_queue_.tryPush(ev);
  });

  handler_->setTelegramCallback(
      [this](MessageType message_type, TelegramType telegram_type,
             ByteView master_view, ByteView slave_view) {
        if (extern_telegram_callback_)
          extern_telegram_callback_(message_type, telegram_type, master_view,
                                    slave_view);

        uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);
        if (id == 0) return;

        Event ev;
        ev.type = EventType::telegram;
        ev.id = id;
        ev.message_type = message_type;
        ev.telegram_type = telegram_type;
        ev.master.assign(master_view);
        ev.slave.assign(slave_view);
        event_queue_.tryPush(ev);
      });

  handler_->setErrorCallback([this](std::string_view error,
                                    ByteView master_view, ByteView slave_view) {
    if (extern_error_callback_)
      extern_error_callback_(error, master_view, slave_view);

    // auto state = handler_->getState();

    // Reset on certain error conditions that indicate the bus is now free
    // again
    uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::error;
    ev.id = id;
    ev.error = error.data();  // Points to the error message literal
    ev.master.assign(master_view);
    ev.slave.assign(slave_view);
    event_queue_.tryPush(ev);
  });
}

void ebus::Scheduler::detachHandlerCallbacks() {
  if (!handler_) return;
  handler_->setBusRequestWonCallback(nullptr);
  handler_->setBusRequestLostCallback(nullptr);
  handler_->setReactiveMasterSlaveCallback(nullptr);
  handler_->setTelegramCallback(nullptr);
  handler_->setErrorCallback(nullptr);
}
