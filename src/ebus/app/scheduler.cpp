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
    : handler_(handler), next_session_id_(1) {}

Scheduler::~Scheduler() { detachHandlerCallbacks(); }

void Scheduler::stop() {
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    active_item_.reset();
  }
  clear();
}

void Scheduler::setEventSink(EventSink sink) { event_sink_ = std::move(sink); }

void Scheduler::setMaxSendAttempts(uint8_t max_send_attempts) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  max_send_attempts_ = max_send_attempts;
  if (max_send_attempts_ == 0) max_send_attempts_ = 1;
}

void Scheduler::setBaseBackoff(uint32_t base_backoff_ms) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  base_backoff_ = std::chrono::milliseconds(base_backoff_ms);
}

void Scheduler::setFsmTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  fsm_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setTotalTimeout(uint32_t timeout_ms) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  total_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setReactiveMasterSlaveCallback(
    ReactiveMasterSlaveCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  extern_reactive_callback_ = std::move(callback);
}

void Scheduler::setTelegramCallback(TelegramCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  extern_telegram_callback_ = std::move(callback);
}

void Scheduler::setErrorCallback(ErrorCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  extern_error_callback_ = std::move(callback);
}

void Scheduler::attachHandlerCallbacks() {
  if (!handler_) return;

  handler_->setBusRequestWonCallback([this]() {
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);
    if (s_id == 0) return;

    ProtocolEvent ev{};
    ev.type = ProtocolEvent::Type::won;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.handler_state = handler_->getState();
    ev.request_state = RequestState::observe;

    if (event_sink_) {
      OrchestrationEvent oev;
      oev.type = OrchestrationEventType::protocol_result;
      oev.data.protocol_data = ev;
      event_sink_(std::move(oev));
    }
  });

  handler_->setBusRequestLostCallback([this]() {
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);
    if (s_id == 0) return;

    ProtocolEvent ev{};
    ev.type = ProtocolEvent::Type::lost;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.handler_state = handler_->getState();
    ev.request_state = RequestState::observe;

    if (event_sink_) {
      OrchestrationEvent oev;
      oev.type = OrchestrationEventType::protocol_result;
      oev.data.protocol_data = ev;
      event_sink_(std::move(oev));
    }
  });

  handler_->setReactiveMasterSlaveCallback([this](const ReactiveInfo& info) {
    ReactiveMasterSlaveCallback user_callback;
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      user_callback = extern_reactive_callback_;
    }
    if (user_callback) {
      user_callback(info);
    }
  });

  handler_->setTelegramCallback([this](const TelegramInfo& info) {
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);

    TelegramCallback user_callback;
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
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

    ProtocolEvent ev;
    ev.type = ProtocolEvent::Type::telegram;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.data.tel.message_type = info.message_type;
    ev.data.tel.telegram_type = info.telegram_type;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.master.assign(info.master_view.data(), info.master_view.size());
    ev.slave.assign(info.slave_view.data(), info.slave_view.size());

    if (event_sink_) {
      OrchestrationEvent oev;
      oev.type = OrchestrationEventType::protocol_result;
      oev.data.protocol_data = ev;
      event_sink_(std::move(oev));
    }
  });

  handler_->setErrorCallback([this](const ErrorInfo& info) {
    // Reset on certain error conditions that indicate the bus is now free
    // again
    uint32_t s_id = current_session_id_.load(std::memory_order_relaxed);
    uint32_t p_id = current_poll_id_.load(std::memory_order_relaxed);
    if (extern_error_callback_) {
      ErrorCallback user_callback;
      {
        std::lock_guard<std::mutex> lock(callback_mutex_);
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

    ProtocolEvent ev;
    ev.type = ProtocolEvent::Type::error;
    ev.session_id = s_id;
    ev.poll_id = p_id;
    ev.data.err.level = info.level;
    ev.data.err.result = info.result;
    ev.data.err.sequence_state = info.sequence_state;
    ev.data.err.protocol_error = info.protocol_error;
    ev.handler_state = info.handler_state;
    ev.request_state = info.request_state;
    ev.master.assign(info.master_view.data(), info.master_view.size());
    ev.slave.assign(info.slave_view.data(), info.slave_view.size());

    if (event_sink_) {
      OrchestrationEvent oev;
      oev.type = OrchestrationEventType::protocol_result;
      oev.data.protocol_data = ev;
      event_sink_(std::move(oev));
    }
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

bool Scheduler::injectProtocolEvent(const ProtocolEvent& event) {
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!active_item_ || event.session_id != active_item_->session_id)
      return false;
    if (event.type == ProtocolEvent::Type::won) return true;
  }
  return handleAttemptResult(event);
}

bool Scheduler::tick() {
  std::optional<Item> item_to_start;
  ProtocolEvent timeout_ev{};
  bool has_timeout = false;

  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (active_item_) {
      auto elapsed = Clock::now() - active_item_->start_time;
      if (elapsed > total_timeout_) {
        timeout_ev.type = ProtocolEvent::Type::error;
        timeout_ev.session_id = active_item_->session_id;
        timeout_ev.data.err.protocol_error =
            ProtocolError::total_transfer_timeout;
        timeout_ev.data.err.result = RequestResult::first_error;
        timeout_ev.data.err.sequence_state = handler_->getActiveSequenceState();
        timeout_ev.data.err.level = LogLevel::error;
        timeout_ev.handler_state = handler_->getState();
        timeout_ev.request_state = RequestState::observe;
        timeout_ev.master.assign(active_item_->item.message.data(),
                                 active_item_->item.message.size());
        has_timeout = true;
      }
    } else if (!scheduled_items_.empty() &&
               scheduled_items_.front().due <= Clock::now()) {
      if (handler_->isActiveMessagePending()) return false;
      std::pop_heap(scheduled_items_.begin(), scheduled_items_.end(),
                    Compare());
      item_to_start = std::move(scheduled_items_.back());
      scheduled_items_.pop_back();
      current_session_id_.store(item_to_start->session_id,
                                std::memory_order_relaxed);
      current_poll_id_.store(item_to_start->poll_id, std::memory_order_relaxed);
    }
  }

  if (has_timeout) {
    handler_->reset();
    handleAttemptResult(timeout_ev);
    return true;
  }

  if (item_to_start) {
    {
      // Correlation FIX: Set active_item_ BEFORE calling the handler.
      // Ensures immediate terminal results (structural errors) map correctly.
      std::lock_guard<std::mutex> lock(data_mutex_);
      active_item_ = {*item_to_start, Clock::now(), item_to_start->session_id};
    }

    if (!handler_->sendActiveMessage(item_to_start->message)) {
      ProtocolEvent fail_ev{};
      fail_ev.type = ProtocolEvent::Type::error;
      fail_ev.session_id = item_to_start->session_id;
      fail_ev.poll_id = item_to_start->poll_id;
      fail_ev.data.err.protocol_error = ProtocolError::invalid_message;
      fail_ev.data.err.result = RequestResult::first_error;
      fail_ev.data.err.sequence_state = handler_->getActiveSequenceState();
      fail_ev.data.err.level = LogLevel::error;
      fail_ev.handler_state = handler_->getState();
      fail_ev.request_state = RequestState::observe;
      handleAttemptResult(fail_ev);
    }
    return true;
  }
  return false;
}

Clock::time_point Scheduler::nextDueTime() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (active_item_) {
    return active_item_->start_time + total_timeout_;
  }
  if (scheduled_items_.empty()) return Clock::time_point::max();
  return scheduled_items_.front().due;
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

SchedulerStatus Scheduler::getStatus() {
  return SchedulerStatus{queueSize(), queueCapacity(), max_queue_size_};
}

void Scheduler::resetPeakMetrics() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  max_queue_size_ = scheduled_items_.size();
}

bool Scheduler::pushItem(Item&& it) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  if (scheduled_items_.size() >= SchedulerLimits::max_items) {
    return false;  // Queue is full
  }
  scheduled_items_.push_back(std::move(it));
  if (scheduled_items_.size() > max_queue_size_)
    max_queue_size_ = scheduled_items_.size();
  std::push_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
  return true;  // Successfully pushed
}

bool Scheduler::handleAttemptResult(const ProtocolEvent& ev) {
  ResultCallback result_callback = nullptr;
  ErrorCallback error_callback = nullptr;
  ResultInfo result_info{};
  ErrorInfo error_info{};
  Sequence slave_res;
  bool has_result = false;
  bool has_error = false;
  {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (!active_item_) return false;
    bool success = false;
    if (ev.type == ProtocolEvent::Type::telegram) {
      success = true;
      slave_res.assign(ev.slave);
    } else if (ev.type == ProtocolEvent::Type::lost ||
               ev.type == ProtocolEvent::Type::error) {
      // Structural protocol errors are not transient; do not retry.
      const bool is_fatal =
          (ev.type == ProtocolEvent::Type::error &&
           ev.data.err.protocol_error == ProtocolError::invalid_message);

      active_item_->item.send_attempts++;
      if (!is_fatal && active_item_->item.send_attempts < max_send_attempts_) {
        // Reschedule with backoff
        active_item_->item.due =
            Clock::now() + backoffDuration(active_item_->item.send_attempts);
        scheduled_items_.push_back(std::move(active_item_->item));
        std::push_heap(scheduled_items_.begin(), scheduled_items_.end(),
                       Compare());
        active_item_.reset();
        current_session_id_.store(0, std::memory_order_relaxed);
        current_poll_id_.store(0, std::memory_order_relaxed);
        return true;
      } else {
        // Final failure
      }
    }

    auto terminal_item = std::move(active_item_->item);
    active_item_.reset();
    current_session_id_.store(0, std::memory_order_relaxed);
    current_poll_id_.store(0, std::memory_order_relaxed);

    if (success) {
      result_callback = std::move(terminal_item.result_callback);
      result_info = ResultInfo{terminal_item.session_id,
                               terminal_item.poll_id,
                               true,
                               RequestResult::first_won,
                               SequenceState::seq_ok,
                               terminal_item.message,
                               slave_res};
      has_result = true;
    } else {
      {
        std::lock_guard<std::mutex> cb_lock(callback_mutex_);
        error_callback = extern_error_callback_;
      }
      error_info =
          ErrorInfo{terminal_item.session_id, terminal_item.poll_id,
                    ev.data.err.level,        ev.data.err.protocol_error,
                    ev.data.err.result,       ev.data.err.sequence_state,
                    ev.handler_state,         ev.request_state,
                    terminal_item.message,    {}};
      has_error = true;
      result_callback = std::move(terminal_item.result_callback);
      result_info = ResultInfo{terminal_item.session_id,
                               terminal_item.poll_id,
                               false,
                               ev.data.err.result,
                               ev.data.err.sequence_state,
                               terminal_item.message,
                               {}};
      has_result = true;
    }
  }
  if (has_error && error_callback) {
    error_callback(error_info);
  }
  if (has_result && result_callback) {
    result_callback(result_info);
  }
  return true;
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

}  // namespace ebus::detail