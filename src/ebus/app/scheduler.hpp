/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

namespace ebus::detail {

/**
 * The Scheduler manages the timing and prioritization of active eBUS messages.
 * It allows enqueuing messages with specific priorities and scheduled times,
 * and handles retries with backoff. The Scheduler runs a worker thread that
 * processes the queue, interacts with the Handler to send messages, and manages
 * callbacks for results, telegrams, and errors. It ensures thread safety for
 * concurrent access and provides configuration options for send attempts and
 * backoff durations. The Scheduler also forwards relevant events to external
 * callbacks for integration with the Controller's central dispatcher.
 */
class Scheduler {
 public:
  using TimePoint = Clock::time_point;
  using Duration = Clock::duration;

  struct Item {
    uint8_t priority = 0;  // larger = higher priority (e.g. 255 is top)
    TimePoint due = Clock::now();
    uint32_t session_id = 0;
    uint32_t poll_id = 0;
    int send_attempts = 0;
    Sequence message;
    ResultCallback result_callback = nullptr;
  };

  explicit Scheduler(Handler* handler);
  ~Scheduler();

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  void start();
  void stop();

  bool enqueue(uint8_t priority, ByteView message,
               ResultCallback callback = nullptr, uint32_t poll_id = 0);
  bool enqueueAt(uint8_t priority, ByteView message, TimePoint when,
                 ResultCallback callback = nullptr, uint32_t poll_id = 0);

  void setMaxSendAttempts(uint8_t send_attempts);
  void setBaseBackoff(uint32_t base_backoff_ms);
  void setFsmTimeout(uint32_t timeout_ms);
  void setTotalTimeout(uint32_t timeout_ms);

  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);
  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  void clear();

  size_t queueSize();
  size_t queueCapacity() const;

  platform::ServiceThread::Status getThreadStatus() const;

  SchedulerStatus getStatus();

 private:
  struct Compare {
    bool operator()(Item const& lhs, Item const& rhs) const {
      // Group items enqueued within the jitter window as logically concurrent
      const auto diff =
          (lhs.due > rhs.due) ? (lhs.due - rhs.due) : (rhs.due - lhs.due);
      if (diff >
          std::chrono::milliseconds(SchedulerLimits::jitter_threshold_ms)) {
        return lhs.due > rhs.due;  // Earlier due time first
      }

      // Within the jitter window, higher priority items are "greater" (top of
      // heap)
      if (lhs.priority != rhs.priority) {
        return lhs.priority < rhs.priority;
      }
      // Maintain stability via session ID if everything else is equal
      return lhs.session_id > rhs.session_id;
    }
  };

  enum class EventType { won, lost, telegram, error };

  struct Event {
    EventType type;
    uint32_t session_id;
    uint32_t poll_id;
    MessageType message_type;
    TelegramType telegram_type;
    HandlerState handler_state;
    RequestState request_state;
    LogLevel level;
    RequestResult result;
    SequenceState sequence_state;
    Sequence master;
    Sequence slave;
    ProtocolError protocol_error = ProtocolError::none;
  };

  Handler* handler_ = nullptr;

  // Queue management
  std::vector<Item> scheduled_items_;
  std::mutex data_mutex_;
  std::condition_variable data_ready_cv_;

  // Worker thread
  std::unique_ptr<platform::ServiceThread> worker_;
  std::atomic<bool> stop_flag_;
  std::atomic<uint32_t> next_session_id_;

  // Active transfer state
  std::atomic<uint32_t> current_session_id_{0};
  std::atomic<uint32_t> current_poll_id_{0};
  platform::Queue<Event> event_queue_{SchedulerLimits::queue_reserve};

  // Configuration
  uint8_t max_send_attempts_ =
      ebus::RuntimeConfig{}.scheduler.max_send_attempts;
  Duration base_backoff_ = std::chrono::milliseconds(
      ebus::RuntimeConfig{}.scheduler.base_backoff_ms);
  std::chrono::milliseconds fsm_timeout_ =
      std::chrono::milliseconds(ebus::RuntimeConfig{}.scheduler.fsm_timeout_ms);
  std::chrono::milliseconds total_timeout_ = std::chrono::milliseconds(
      ebus::RuntimeConfig{}.scheduler.total_timeout_ms);

  // Forwarded callbacks
  ReactiveMasterSlaveCallback extern_reactive_callback_ = nullptr;
  TelegramCallback extern_telegram_callback_ = nullptr;
  ErrorCallback extern_error_callback_ = nullptr;

  bool pushItem(Item&& it);
  void run();

  Duration backoffDuration(int attempt) const;
  void attachHandlerCallbacks();
  void detachHandlerCallbacks();
};

}  // namespace ebus::detail
