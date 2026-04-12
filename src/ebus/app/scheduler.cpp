/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/scheduler.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "utils/common.hpp"

ebus::Scheduler::Scheduler(Handler* handler)
    : handler_(handler),
      stopFlag_(true),
      nextId_(1),
      maxSendAttempts_(3),
      baseBackoff_(std::chrono::milliseconds(100)) {
  attachHandlerCallbacks();
}

ebus::Scheduler::~Scheduler() { stop(); }

void ebus::Scheduler::start() {
  bool expected = true;
  if (stopFlag_.compare_exchange_strong(expected, false)) {
    worker_ = std::make_unique<ServiceThread>(
        "ebusScheduler", [this] { run(); }, 4096, 5, 1);
    worker_->start();
  }
}

void ebus::Scheduler::stop() {
  bool expected = false;
  if (stopFlag_.compare_exchange_strong(expected, true)) {
    {
      std::lock_guard<std::mutex> lock(dataMutex_);
      dataReadyCv_.notify_all();
    }

    detachHandlerCallbacks();

    if (worker_) worker_->join();
  }
}

void ebus::Scheduler::enqueue(uint8_t priority,
                              const std::vector<uint8_t>& message,
                              ResultCallback callback) {
  Item it;
  it.priority = priority;
  it.due = Clock::now();
  it.message = message;
  it.resultCallback = std::move(callback);
  it.id = nextId_++;
  pushItem(std::move(it));
}

void ebus::Scheduler::enqueueAt(uint8_t priority,
                                const std::vector<uint8_t>& message,
                                TimePoint when, ResultCallback callback) {
  Item it;
  it.priority = priority;
  it.due = when;
  it.message = message;
  it.resultCallback = std::move(callback);
  it.id = nextId_++;
  pushItem(std::move(it));
}

void ebus::Scheduler::setMaxSendAttempts(int sendAttempts) {
  maxSendAttempts_ = std::max(1, sendAttempts);
}

void ebus::Scheduler::setBaseBackoff(Duration duration) {
  baseBackoff_ = duration;
}

void ebus::Scheduler::setTelegramCallback(TelegramCallback callback) {
  externTelegramCallback_ = std::move(callback);
}

void ebus::Scheduler::setErrorCallback(ErrorCallback callback) {
  externErrorCallback_ = std::move(callback);
}

size_t ebus::Scheduler::queueSize() {
  std::lock_guard<std::mutex> lock(dataMutex_);
  return itemQueue_.size();
}

void ebus::Scheduler::clear() {
  std::lock_guard<std::mutex> lock(dataMutex_);
  itemQueue_.clear();
  std::make_heap(itemQueue_.begin(), itemQueue_.end(), Compare());
}

void ebus::Scheduler::pushItem(Item&& it) {
  std::lock_guard<std::mutex> lock(dataMutex_);
  itemQueue_.push_back(std::move(it));
  std::push_heap(itemQueue_.begin(), itemQueue_.end(), Compare());
  dataReadyCv_.notify_one();
}

void ebus::Scheduler::run() {
  std::unique_lock<std::mutex> lock(dataMutex_);

  // Main loop: wait for next due item, attempt to send, and handle retries if
  // needed
  while (!stopFlag_.load()) {
    if (itemQueue_.empty()) {
      dataReadyCv_.wait(
          lock, [this] { return stopFlag_.load() || !itemQueue_.empty(); });
      if (stopFlag_.load()) break;
    }

    // copy next due while holding lock
    auto next_due = itemQueue_.front().due;

    dataReadyCv_.wait_until(lock, next_due, [this, next_due] {
      return stopFlag_.load() || itemQueue_.empty() ||
             itemQueue_.front().due <= Clock::now() ||
             itemQueue_.front().due < next_due;
    });

    if (stopFlag_.load()) break;
    if (itemQueue_.empty()) continue;
    if (itemQueue_.front().due > Clock::now()) continue;

    std::pop_heap(itemQueue_.begin(), itemQueue_.end(), Compare());
    Item currentItem = std::move(itemQueue_.back());
    itemQueue_.pop_back();

    lock.unlock();

    bool sent = false;
    std::string lastError = "unknown";
    std::vector<uint8_t> slaveResponse;
    uint32_t attemptId = currentItem.id;

    currentAttemptId_.store(attemptId, std::memory_order_relaxed);

    // Clear any stale events from previous attempts
    Event stale;
    while (eventQueue_.tryPop(stale));

    if (stopFlag_.load()) break;

    // Initiation: check if handler is busy, then arm it.
    if (handler_->isActiveMessagePending()) {
      lastError = "Handler busy";
    } else if (!handler_->sendActiveMessage(currentItem.message)) {
      lastError = "Invalid message";
    }

    // If arming succeeded, wait for terminal events
    if (lastError == "unknown") {
      auto start = Clock::now();
      while (!stopFlag_.load()) {
        Event ev;
        if (!eventQueue_.pop(ev, 2000)) {
          lastError = "FSM timeout";
          handler_->reset();  // Serious error: FSM stuck.
          break;
        }

        if (ev.id != attemptId) continue;

        if (ev.type == EventType::Telegram) {
          sent = true;
          slaveResponse = std::move(ev.slave);
          break;
        } else if (ev.type == EventType::Lost) {
          lastError = "Arbitration lost";
          break;
        } else if (ev.type == EventType::Error) {
          lastError = ev.error;
          break;
        }

        if (Clock::now() - start > std::chrono::seconds(4)) {
          lastError = "Total transfer timeout";
          handler_->reset();
          break;
        }
      }
    }

    // clear attempt id so stray callbacks are ignored
    currentAttemptId_.store(0, std::memory_order_relaxed);

    if (sent) {
      if (currentItem.resultCallback)
        currentItem.resultCallback(true, currentItem.message, slaveResponse);
      lock.lock();
    } else {
      ++currentItem.sendAttempts;
      if (currentItem.sendAttempts < maxSendAttempts_) {
        currentItem.due =
            Clock::now() + backoffDuration(currentItem.sendAttempts);
        lock.lock();
        itemQueue_.push_back(std::move(currentItem));
        std::push_heap(itemQueue_.begin(), itemQueue_.end(), Compare());
        dataReadyCv_.notify_one();
      } else {
        if (externErrorCallback_) {
          externErrorCallback_(lastError, currentItem.message, {});
        }
        if (currentItem.resultCallback) {
          currentItem.resultCallback(false, currentItem.message, {});
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

  // multiply baseBackoff_ by (1 << shift) using integer multiplication on
  // count() to avoid accidental scaling issues with some duration types.
  using Rep = typename Duration::rep;

  // Cap shift to prevent overflow if Rep is small or base is large
  if (shift >= static_cast<int>(sizeof(Rep) * 8 - 2)) return Duration::max();

  Rep factor = static_cast<Rep>(1) << shift;
  return Duration(static_cast<Rep>(baseBackoff_.count() * factor));
}

void ebus::Scheduler::attachHandlerCallbacks() {
  if (!handler_) return;

  handler_->setBusRequestWonCallback([this]() {
    uint32_t id = currentAttemptId_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::Won;
    ev.id = id;
    eventQueue_.tryPush(ev);
  });

  handler_->setBusRequestLostCallback([this]() {
    uint32_t id = currentAttemptId_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::Lost;
    ev.id = id;
    eventQueue_.tryPush(ev);
  });

  handler_->setTelegramCallback([this](const MessageType& messageType,
                                       const TelegramType& telegramType,
                                       const std::vector<uint8_t>& master,
                                       const std::vector<uint8_t>& slave) {
    if (externTelegramCallback_)
      externTelegramCallback_(messageType, telegramType, master, slave);

    uint32_t id = currentAttemptId_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::Telegram;
    ev.id = id;
    ev.messageType = messageType;
    ev.telegramType = telegramType;
    ev.master = master;
    ev.slave = slave;
    eventQueue_.tryPush(ev);
  });

  handler_->setErrorCallback([this](const std::string& error,
                                    const std::vector<uint8_t>& master,
                                    const std::vector<uint8_t>& slave) {
    if (externErrorCallback_) externErrorCallback_(error, master, slave);

    // auto state = handler_->getState();

    // Reset on certain error conditions that indicate the bus is now free again
    uint32_t id = currentAttemptId_.load(std::memory_order_relaxed);
    if (id == 0) return;

    Event ev;
    ev.type = EventType::Error;
    ev.id = id;
    ev.error = error;
    ev.master = master;
    ev.slave = slave;
    eventQueue_.tryPush(ev);
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
