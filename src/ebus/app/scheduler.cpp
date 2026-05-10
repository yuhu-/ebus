/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/scheduler.hpp"

#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <memory>

namespace ebus::detail {

Scheduler::Scheduler(Handler* handler)
    : handler_(handler), stop_flag_(true), next_session_id_(1) {
  // Reserve space for typical traffic bursts
  scheduled_items_.reserve(SchedulerLimits::queue_reserve);
  attachHandlerCallbacks();
}

Scheduler::~Scheduler() { stop(); }

void Scheduler::start() {
  bool expected = true;
  if (stop_flag_.compare_exchange_strong(expected, false)) {
    worker_ = std::make_unique<platform::ServiceThread>(
        "ebus_scheduler", [this] { run(); },
        OrchestrationLimits::scheduler_stack_size,
        OrchestrationLimits::scheduler_priority);
    worker_->start();
  }
}

void Scheduler::stop() {
  bool expected = false;
  if (stop_flag_.compare_exchange_strong(expected, true)) {
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      data_ready_cv_.notify_all();
    }
    event_queue_.shutdown();
    clear();  // Clear any pending items and their callbacks to prevent UAF
              // during test teardown

    detachHandlerCallbacks();

    if (worker_) worker_->join();
  }
}

bool Scheduler::enqueue(uint8_t priority, ByteView message,
                        ResultCallback callback, uint32_t poll_id) {
  Item it;
  it.priority = priority;
  it.due = Clock::now();
  it.message.assign(message);
  it.result_callback = std::move(callback);
  it.session_id = next_session_id_++;
  it.poll_id = poll_id;
  return pushItem(std::move(it));
}

bool Scheduler::enqueueAt(uint8_t priority, ByteView message, TimePoint when,
                          ResultCallback callback, uint32_t poll_id) {
  Item it;
  it.priority = priority;
  it.due = when;
  it.message.assign(message);
  it.result_callback = std::move(callback);
  it.session_id = next_session_id_++;
  it.poll_id = poll_id;
  return pushItem(std::move(it));
}

void Scheduler::setMaxSendAttempts(uint8_t max_send_attempts) {
  max_send_attempts_ = max_send_attempts;
  if (max_send_attempts_ == 0) max_send_attempts_ = 1;
}

void Scheduler::setBaseBackoff(uint32_t base_backoff_ms) {
  base_backoff_ = std::chrono::milliseconds(base_backoff_ms);
}

void Scheduler::setFsmTimeout(uint32_t timeout_ms) {
  fsm_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setTotalTimeout(uint32_t timeout_ms) {
  total_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  extern_reactive_callback_ = std::move(callback);
}

void Scheduler::setTelegramCallback(TelegramCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  extern_telegram_callback_ = std::move(callback);
}

void Scheduler::setErrorCallback(ErrorCallback callback) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  extern_error_callback_ = std::move(callback);
}

void Scheduler::clear() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  scheduled_items_.clear();
  std::make_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
}

size_t Scheduler::queueSize() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return scheduled_items_.size();
}

size_t Scheduler::queueCapacity() const { return SchedulerLimits::max_items; }

platform::ServiceThread::Status Scheduler::getThreadStatus() const {
  if (worker_) {
    return worker_->status();
  }
  return platform::ServiceThread::Status{"ebus_scheduler", -1, -1};
}

SchedulerStatus Scheduler::getStatus() {
  auto s = getThreadStatus();
  return {{s.name, s.task_stack_bytes, s.task_stack_free_bytes},
          queueSize(),
          queueCapacity()};
}

bool Scheduler::pushItem(Item&& it) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (scheduled_items_.size() >= SchedulerLimits::max_items) {
    return false;  // Queue is full
  }
  scheduled_items_.push_back(std::move(it));
  std::push_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
  data_ready_cv_.notify_one();
  return true;  // Successfully pushed
}

void Scheduler::run() {
  std::unique_lock<std::mutex> lock(data_mutex_);

  // Main loop: wait for next due item, attempt to send, and handle retries if
  // needed
  while (!stop_flag_.load()) {
    if (scheduled_items_.empty()) {
      data_ready_cv_.wait(lock, [this] {
        return stop_flag_.load() || !scheduled_items_.empty();
      });
      if (stop_flag_.load()) break;
    }

    // copy next due while holding lock
    auto next_due = scheduled_items_.front().due;
    auto next_session_id = scheduled_items_.front().session_id;

    data_ready_cv_.wait_until(
        lock, next_due, [this, next_due, next_session_id] {
          return stop_flag_.load() || scheduled_items_.empty() ||
                 scheduled_items_.front().due <= Clock::now() ||
                 scheduled_items_.front().due < next_due ||
                 scheduled_items_.front().session_id != next_session_id;
        });

    if (stop_flag_.load()) break;
    if (scheduled_items_.empty()) continue;
    if (scheduled_items_.front().due > Clock::now()) continue;

    std::pop_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
    Item current_item = std::move(scheduled_items_.back());
    scheduled_items_.pop_back();

    lock.unlock();

    bool sent = false;
    ProtocolError last_error_code = ProtocolError::none;
    LogLevel result_level = LogLevel::error;
    RequestResult result_code = RequestResult::first_error;
    SequenceState result_s_state = SequenceState::seq_ok;
    HandlerState result_h_state = HandlerState::passive_receive_master;
    RequestState result_r_state = RequestState::observe;
    Sequence slave_response;
    uint32_t attempt_session_id = current_item.session_id;
    uint32_t attempt_poll_id = current_item.poll_id;

    current_session_id_.store(attempt_session_id, std::memory_order_relaxed);
    current_poll_id_.store(attempt_poll_id, std::memory_order_relaxed);

    // Clear any stale events from previous attempts
    Event stale;
    while (event_queue_.tryPop(stale));

    if (stop_flag_.load()) break;

    // Initiation: check if handler is busy, then arm it.
    if (handler_->isActiveMessagePending()) {
      last_error_code = ProtocolError::handler_busy;
    } else if (!handler_->sendActiveMessage(current_item.message)) {
      last_error_code = ProtocolError::invalid_message;
      result_s_state = handler_->getActiveSequenceState();
      // Structural error: structure or address invalid. Retry will not help.
      current_item.send_attempts = max_send_attempts_;
    }

    // If arming succeeded (no immediate error), wait for terminal events
    if (last_error_code == ProtocolError::none) {
      auto start = Clock::now();
      while (!stop_flag_.load()) {
        Event ev;
        if (!event_queue_.pop(ev,
                              static_cast<uint32_t>(fsm_timeout_.count()))) {
          last_error_code = ProtocolError::fsm_timeout;
          handler_->reset();  // Serious error: FSM stuck.
          break;
        }

        if (ev.session_id != attempt_session_id) continue;

        if (ev.type == EventType::telegram) {
          sent = true;
          slave_response = std::move(ev.slave);
          break;
        } else if (ev.type == EventType::lost) {
          last_error_code = ProtocolError::arbitration_lost;
          break;
        } else if (ev.type == EventType::error) {
          last_error_code = ev.protocol_error;
          result_code = ev.result;
          result_s_state = ev.sequence_state;
          result_level = ev.level;
          result_h_state = ev.handler_state;
          result_r_state = ev.request_state;
          break;
        }

        if (Clock::now() - start > total_timeout_) {
          last_error_code = ProtocolError::total_transfer_timeout;
          handler_->reset();
          break;
        }
      }
    }

    // clear attempt id so stray callbacks are ignored
    current_session_id_.store(0, std::memory_order_relaxed);
    current_poll_id_.store(0, std::memory_order_relaxed);

    /**
     * Reliability Logic:
     * The eBUS specification (Section 7.4) restricts immediate repetitions
     * (on NAK) to a repeat rate of 1. This library complies by handling the
     * physical retry inside the Handler FSM.
     * The Scheduler implements application-level retries with exponential
     * backoff, which occurs after the bus has been released.
     */
    if (sent) {
      if (current_item.result_callback)
        current_item.result_callback(
            {current_item.session_id, current_item.poll_id, true,
             RequestResult::first_won, SequenceState::seq_ok,
             current_item.message, slave_response});
      lock.lock();
    } else {
      // Capture callbacks under lock for consistent access
      ErrorCallback error_cb;
      {
        std::lock_guard<std::mutex> cb_lock(data_mutex_);
        error_cb = extern_error_callback_;
      }
      ++current_item.send_attempts;
      if (current_item.send_attempts < max_send_attempts_) {
        current_item.due =
            Clock::now() + backoffDuration(current_item.send_attempts);
        lock.lock();
        scheduled_items_.push_back(std::move(current_item));
        std::push_heap(scheduled_items_.begin(), scheduled_items_.end(),
                       Compare());
        data_ready_cv_.notify_one();
      } else {
        if (error_cb) {
          error_cb({current_item.session_id,
                    current_item.poll_id,
                    result_level,  // LogLevel is still used for filtering
                    last_error_code,
                    result_code,
                    result_s_state,
                    result_h_state,
                    result_r_state,
                    current_item.message,
                    {}});
        }
        if (current_item.result_callback) {
          current_item.result_callback({current_item.session_id,
                                        current_item.poll_id,
                                        false,
                                        result_code,
                                        result_s_state,
                                        current_item.message,
                                        {}});
        }
        lock.lock();
      }
    }
  }
}

Scheduler::Duration Scheduler::backoffDuration(int attempt) const {
  // Pre-calculated multipliers for 2^(attempt-1) to avoid runtime bit-shifts.
  using Rep = typename Duration::rep;
  static constexpr Rep kMultipliers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
  constexpr int kMaxAttempt = sizeof(kMultipliers) / sizeof(kMultipliers[0]);

  if (attempt <= 0) return Duration::zero();
  if (attempt > kMaxAttempt) return Duration::max();

  Rep factor = kMultipliers[attempt - 1];
  return Duration(static_cast<Rep>(base_backoff_.count() * factor));
}

void Scheduler::attachHandlerCallbacks() {
  if (!handler_) return;

  handler_->setBusRequestWonCallback([this]() {
    if (stop_flag_.load(std::memory_order_relaxed)) return;
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);
    if (s_id == 0) return;

    Event ev;
    ev.type = EventType::won;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    event_queue_.tryPush(ev);
  });

  handler_->setBusRequestLostCallback([this]() {
    if (stop_flag_.load(std::memory_order_relaxed)) return;
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);
    if (s_id == 0) return;

    Event ev;
    ev.type = EventType::lost;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    event_queue_.tryPush(ev);
  });

  handler_->setReactiveMasterSlaveCallback([this](const ReactiveInfo& info) {
    if (stop_flag_.load(std::memory_order_relaxed)) return;
    ReactiveMasterSlaveCallback user_callback;
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      user_callback = extern_reactive_callback_;
    }
    if (user_callback) {
      user_callback(info);
    }
  });

  handler_->setTelegramCallback([this](const TelegramInfo& info) {
    if (stop_flag_.load(std::memory_order_relaxed)) return;
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);

    TelegramCallback user_callback;
    {
      std::lock_guard<std::mutex> lock(data_mutex_);
      user_callback = extern_telegram_callback_;
    }

    if (user_callback) {
      if (s_id != 0 && info.message_type == MessageType::active) {
        auto correlated = info;
        correlated.session_id = s_id;
        correlated.poll_id = p_id;
        user_callback(correlated);
      } else {
        user_callback(info);
      }
    }

    if (s_id == 0) return;

    Event ev;
    ev.type = EventType::telegram;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.message_type = info.message_type;
    ev.telegram_type = info.telegram_type;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.master.assign(info.master_view);
    ev.slave.assign(info.slave_view);
    event_queue_.tryPush(ev);
  });

  handler_->setErrorCallback([this](const ErrorInfo& info) {
    if (stop_flag_.load(std::memory_order_relaxed)) return;
    // Reset on certain error conditions that indicate the bus is now free
    // again
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);

    if (extern_error_callback_) {
      ErrorCallback user_callback;
      {
        std::lock_guard<std::mutex> lock(data_mutex_);
        user_callback = extern_error_callback_;
      }

      if (s_id != 0) {
        auto correlated = info;
        correlated.session_id = s_id;
        correlated.poll_id = p_id;
        user_callback(correlated);
      } else {
        user_callback(info);
      }
    }
    if (s_id == 0) return;

    Event ev;
    ev.type = EventType::error;  // EventType::error is still used
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.level = info.level;
    ev.result = info.result;
    ev.sequence_state = info.sequence_state;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.protocol_error = info.protocol_error;
    ev.master.assign(info.master_view);
    ev.slave.assign(info.slave_view);
    event_queue_.tryPush(ev);
  });
}

void Scheduler::detachHandlerCallbacks() {
  if (!handler_) return;
  handler_->setBusRequestWonCallback(nullptr);
  handler_->setBusRequestLostCallback(nullptr);
  handler_->setReactiveMasterSlaveCallback(nullptr);
  handler_->setTelegramCallback(nullptr);
  handler_->setErrorCallback(nullptr);
}

}  // namespace ebus::detail