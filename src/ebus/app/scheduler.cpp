/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/scheduler.hpp"

#include <algorithm>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <memory>

ebus::Scheduler::Scheduler(Handler* handler)
    : handler_(handler), stop_flag_(true), next_id_(1) {
  // Reserve space for typical traffic bursts
  item_queue_.reserve(defaults::Scheduler::queue_reserve);
  attachHandlerCallbacks();
}

ebus::Scheduler::~Scheduler() { stop(); }

void ebus::Scheduler::start() {
  bool expected = true;
  if (stop_flag_.compare_exchange_strong(expected, false)) {
    worker_ = std::make_unique<ServiceThread>(
        "ebusScheduler", [this] { run(); }, defaults::Orchestration::stack_size,
        defaults::Orchestration::priority_high, 1);
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

void ebus::Scheduler::setMaxSendAttempts(int max_send_attempts) {
  max_send_attempts_ = std::max(1, max_send_attempts);
}

void ebus::Scheduler::setBaseBackoff(Duration base_backoff) {
  base_backoff_ = base_backoff;
}

void ebus::Scheduler::setFsmTimeout(std::chrono::milliseconds timeout) {
  fsm_timeout_ms_ = timeout;
}

void ebus::Scheduler::setTotalTimeout(std::chrono::milliseconds timeout) {
  total_timeout_ms_ = timeout;
}

void ebus::Scheduler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  extern_reactive_callback_ = std::move(callback);
  if (handler_)
    handler_->setReactiveMasterSlaveCallback(extern_reactive_callback_);
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
    LogLevel result_level = LogLevel::error;
    RequestResult result_code = RequestResult::first_error;
    HandlerState result_h_state = HandlerState::passive_receive_master;
    RequestState result_r_state = RequestState::observe;
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
        if (!event_queue_.pop(ev,
                              static_cast<uint32_t>(fsm_timeout_ms_.count()))) {
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
          result_code = ev.result;
          result_level = ev.level;
          result_h_state = ev.handler_state;
          result_r_state = ev.request_state;
          break;
        }

        if (Clock::now() - start > total_timeout_ms_) {
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
        current_item.result_callback(
            {current_item.id, true, current_item.message, slave_response});
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
          extern_error_callback_({current_item.id,
                                  result_level,
                                  last_error,
                                  result_code,
                                  result_h_state,
                                  result_r_state,
                                  current_item.message,
                                  {}});
        }
        if (current_item.result_callback) {
          current_item.result_callback(
              {current_item.id, false, current_item.message, {}});
        }
        lock.lock();
      }
    }
  }
}

ebus::Scheduler::Duration ebus::Scheduler::backoffDuration(int attempt) const {
  // Pre-calculated multipliers for 2^(attempt-1) to avoid runtime bit-shifts.
  using Rep = typename Duration::rep;
  static constexpr Rep kMultipliers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
  constexpr int kMaxAttempt = sizeof(kMultipliers) / sizeof(kMultipliers[0]);

  if (attempt <= 0) return Duration::zero();
  if (attempt > kMaxAttempt) return Duration::max();

  Rep factor = kMultipliers[attempt - 1];
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

  handler_->setReactiveMasterSlaveCallback([this](const ReactiveInfo& info) {
    if (extern_reactive_callback_) extern_reactive_callback_(info);
  });

  handler_->setTelegramCallback([this](const TelegramInfo& info) {
    uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);

    if (extern_telegram_callback_) {
      if (id != 0 && info.message_type == MessageType::active) {
        auto correlated = info;
        correlated.session_id = id;
        extern_telegram_callback_(correlated);
      } else {
        extern_telegram_callback_(info);
      }
    }

    if (id == 0) return;

    Event ev;
    ev.type = EventType::telegram;
    ev.id = id;
    ev.message_type = info.message_type;
    ev.telegram_type = info.telegram_type;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.master.assign(info.master);
    ev.slave.assign(info.slave);
    event_queue_.tryPush(ev);
  });

  handler_->setErrorCallback([this](const ErrorInfo& info) {
    // Reset on certain error conditions that indicate the bus is now free
    // again
    uint32_t id = current_attempt_id_.load(std::memory_order_relaxed);

    if (extern_error_callback_) {
      if (id != 0) {
        auto correlated = info;
        correlated.session_id = id;
        extern_error_callback_(correlated);
      } else {
        extern_error_callback_(info);
      }
    }

    if (id == 0) return;

    Event ev;
    ev.type = EventType::error;
    ev.id = id;
    ev.level = info.level;
    ev.result = info.result;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.error = info.message.data();  // Points to the error message literal
    ev.master.assign(info.master);
    ev.slave.assign(info.slave);
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
