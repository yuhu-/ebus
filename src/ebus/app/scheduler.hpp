/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ebus/sequence.hpp>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"

namespace ebus {

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
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = Clock::duration;

  struct Item {
    uint8_t priority = 0;  // larger = higher priority (e.g. 255 is top)
    TimePoint due = Clock::now();
    uint32_t id = 0;
    int send_attempts = 0;
    Sequence message;
    ResultCallback result_callback = nullptr;
  };

  Scheduler(Handler* handler);
  ~Scheduler();

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  void start();
  void stop();

  void enqueue(uint8_t priority, ByteView message,
               ResultCallback callback = nullptr);
  void enqueueAt(uint8_t priority, ByteView message, TimePoint when,
                 ResultCallback callback = nullptr);

  void setMaxSendAttempts(int send_attempts);
  void setBaseBackoff(Duration duration);

  void setTelegramCallback(TelegramCallback callback);
  void setErrorCallback(ErrorCallback callback);

  size_t queueSize();

  void clear();

 private:
  struct Compare {
    bool operator()(Item const& lhs, Item const& rhs) const {
      if (lhs.due != rhs.due)
        return lhs.due > rhs.due;          // earlier due time first
      return lhs.priority < rhs.priority;  // larger priority value second
    }
  };

  enum class EventType { won, lost, telegram, error };

  struct Event {
    EventType type;
    uint32_t id;
    MessageType message_type;
    TelegramType telegram_type;
    Sequence master;
    Sequence slave;
    const char* error = nullptr;
  };

  Handler* handler_ = nullptr;

  // Queue management
  std::vector<Item> item_queue_;
  std::mutex data_mutex_;
  std::condition_variable data_ready_cv_;

  // Worker thread
  std::unique_ptr<ServiceThread> worker_;
  std::atomic<bool> stop_flag_;
  std::atomic<uint32_t> next_id_;

  // Active transfer state
  std::atomic<uint32_t> current_attempt_id_{0};
  Queue<Event> event_queue_{16};
  // Configuration
  int max_send_attempts_;
  Duration base_backoff_;

  // Forwarded callbacks
  TelegramCallback extern_telegram_callback_ = nullptr;
  ErrorCallback extern_error_callback_ = nullptr;

  void pushItem(Item&& it);
  void run();
  Duration backoffDuration(int attempt) const;
  void attachHandlerCallbacks();
  void detachHandlerCallbacks();
};

}  // namespace ebus
