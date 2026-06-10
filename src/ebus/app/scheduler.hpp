/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <ebus/status.hpp>
#include <memory>
#include <optional>

#include "core/handler.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "utils/static_vector.hpp"

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
  // Public Types & Constants
  using TimePoint = Clock::time_point;
  using Duration = Clock::duration; // Duration type for internal use
  using EventSink = platform::Delegate<void(OrchestrationEvent&&)>;

  // Lifecycle
  explicit Scheduler(Handler* handler);
  ~Scheduler();
  void stop();

  // Special Members & Operators
  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  // Configuration
  void setEventSink(EventSink sink);
  void setMaxSendAttempts(uint8_t send_attempts);
  void setBaseBackoff(uint32_t base_backoff_ms);
  void setFsmTimeout(uint32_t timeout_ms);
  void setTotalTimeout(uint32_t timeout_ms);

  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback); // Public API uses std::function
  void setProtocolCallback(ProtocolCallback callback); // Public API uses std::function

  // Working Methods
  void attachHandlerCallbacks();
  void detachHandlerCallbacks();
  /**
   * @brief Injects a protocol result from the Controller's Reactor loop.
   * Bridges the decoupled events to the Scheduler's processing thread.
   * @return true if the event resulted in a state change (e.g. terminal
   * result).
   */
  bool injectProtocolEvent(const ProtocolEvent& event);
  /**
   * @brief Performs periodic maintenance. Returns true if work was done.
   */
  bool tick();
  uint32_t enqueue(uint8_t priority, ByteView message, uint32_t poll_id = 0);
  uint32_t enqueueAt(uint8_t priority, ByteView message, TimePoint when,
                     uint32_t poll_id = 0);
  void clear();

  // Status/Telemetry
  Clock::time_point nextDueTime() const;
  size_t queueSize() const;
  size_t queueCapacity() const;
  SchedulerStatus getStatus() const;
  void resetPeakMetrics();

 private:
  struct Item {
    uint8_t priority = 0;  // larger = higher priority (e.g. 255 is top)
    TimePoint due;         // set during enqueue
    uint32_t session_id = 0;
    uint32_t poll_id = 0;
    int send_attempts = 0;
    Sequence message;
  };

  struct Compare {
    bool operator()(Item const& lhs, Item const& rhs) const {
      static constexpr Duration kJitter =
          std::chrono::milliseconds(SchedulerLimits::jitter_threshold_ms);

      // Group due times into buckets of kJitter size to guarantee a strict weak
      // ordering
      const auto lhs_bucket = lhs.due.time_since_epoch() / kJitter;
      const auto rhs_bucket = rhs.due.time_since_epoch() / kJitter;

      if (lhs_bucket != rhs_bucket) {
        return lhs_bucket > rhs_bucket;  // Earlier bucket wins (Min-Heap
                                         // behavior for due time)
      }

      // 2. Jitter window: within 2ms, priority takes precedence (Max-Heap)
      if (lhs.priority != rhs.priority) {
        return lhs.priority < rhs.priority;
      }

      // 3. FIFO stability: older session ID wins for equal priority
      return lhs.session_id > rhs.session_id;
    }
  };

  Handler* handler_ = nullptr;

  // Queue management
  StaticVector<Item, SchedulerLimits::max_items> scheduled_items_;
  mutable platform::Mutex data_mutex_;
  mutable platform::Mutex callback_mutex_;

  size_t max_queue_size_ = 0;

  struct ActiveAttempt {
    Item item;
    Clock::time_point start_time;
    uint32_t session_id;
  };
  std::optional<ActiveAttempt> active_item_;

  EventSink event_sink_;

  std::atomic<uint32_t> next_session_id_;

  // Active transfer state
  std::atomic<uint32_t> current_session_id_{0};
  std::atomic<uint32_t> current_poll_id_{0};

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
  ProtocolCallback extern_protocol_callback_ = nullptr;

  // Private Helper Methods
  bool pushItem(Item&& it);
  bool handleAttemptResult(const ProtocolEvent& ev);
  Duration backoffDuration(int attempt) const;

  // Handler callback targets
  void onBusRequestWon();
  void onBusRequestLost();
  void onHandlerReactive(const ReactiveInfo& info);
  void onHandlerProtocol(const ProtocolInfo& info);
};

}  // namespace ebus::detail
