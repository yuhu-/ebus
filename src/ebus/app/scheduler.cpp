/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/scheduler.hpp"

#include <algorithm>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <memory>

#include "core/bus_monitor.hpp"
#include "core/handler.hpp"

namespace ebus::detail {

Scheduler::Scheduler(Handler* handler)
    : handler_(handler), next_session_id_(1) {}

Scheduler::~Scheduler() { detachHandlerCallbacks(); }

void Scheduler::stop() {
  {
    platform::LockGuard<platform::Mutex> lock(data_mutex_);
    active_item_.reset();
  }
  clear();
}

void Scheduler::setProtocolEventSink(Delegate<void(ProtocolEvent&&)> sink) {
  event_sink_ = std::move(sink);
}

void Scheduler::setMaxAttempts(uint8_t max_attempts) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  max_attempts_ = max_attempts;
  if (max_attempts_ == 0) max_attempts_ = 1;
}

void Scheduler::setBaseBackoff(uint32_t base_backoff_ms) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  if (base_backoff_ms == 0) base_backoff_ms = 1;
  base_backoff_ = std::chrono::milliseconds(base_backoff_ms);
}

void Scheduler::setFsmTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  fsm_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setTotalTimeout(uint32_t timeout_ms) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  total_timeout_ = std::chrono::milliseconds(timeout_ms);
}

void Scheduler::setBreakerThreshold(uint32_t consecutive_failures) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  breaker_threshold_ = consecutive_failures == 0 ? 1 : consecutive_failures;
}

void Scheduler::setBreakerCooldownMs(uint32_t base_ms, uint32_t max_ms) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  breaker_cooldown_base_ =
      std::chrono::milliseconds(base_ms == 0 ? 1 : base_ms);
  breaker_cooldown_max_ = std::chrono::milliseconds(max_ms == 0 ? 1 : max_ms);
}

bool Scheduler::breakerOpen() const {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  return breakerOpenLocked();
}

uint32_t Scheduler::breakerConsecutiveFailures() const {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  return breaker_consecutive_;
}

uint32_t Scheduler::breakerTrips() const {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  return breaker_trips_;
}

void Scheduler::resetBreaker() {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  breaker_consecutive_ = 0;
  breaker_trips_ = 0;
  breaker_open_until_ = TimePoint{};
  publishBreakerLocked();
}

void Scheduler::setReactiveCallback(ReactiveCallback callback) {
  user_reactive_callback_ = std::move(callback);
}

void Scheduler::attachHandlerCallbacks() {
  if (!handler_) return;

  handler_->setBusRequestWonCallback(
      Delegate<void()>::bind<Scheduler, &Scheduler::onBusRequestWon>(this));
  handler_->setBusRequestLostCallback(
      Delegate<void()>::bind<Scheduler, &Scheduler::onBusRequestLost>(this));
  handler_->setReactiveCallback(
      Delegate<void(const ReactiveInfo&)>::bind<Scheduler,
                                                &Scheduler::onHandlerReactive>(
          this));
  handler_->setProtocolCallback(
      Delegate<void(const ProtocolInfo&)>::bind<Scheduler,
                                                &Scheduler::onHandlerProtocol>(
          this));
}

void Scheduler::detachHandlerCallbacks() {
  if (!handler_) return;
  handler_->setBusRequestWonCallback(nullptr);
  handler_->setBusRequestLostCallback(nullptr);
  handler_->setReactiveCallback(nullptr);
  handler_->setProtocolCallback(nullptr);
}

void Scheduler::onBusRequestWon() {
  uint32_t s_id = current_session_id_.load(std::memory_order_acquire);
  uint16_t p_id = current_poll_id_.load(std::memory_order_acquire);
  if (s_id == 0) return;

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::won;
  ev.session_id = s_id;
  ev.poll_id = p_id;
  ev.handler_state = handler_->getState();
  ev.request_state = RequestState::observe;
  ev.timestamp = ebus::getWallTimeMs();

  if (event_sink_) {
    event_sink_(std::move(ev));
  }
}

void Scheduler::onBusRequestLost() {
  uint32_t s_id = current_session_id_.load(std::memory_order_acquire);
  uint16_t p_id = current_poll_id_.load(std::memory_order_acquire);
  if (s_id == 0) return;

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::lost;
  ev.session_id = s_id;
  ev.poll_id = p_id;
  ev.handler_state = handler_->getState();
  ev.request_state = RequestState::observe;
  ev.timestamp = ebus::getWallTimeMs();

  if (event_sink_) {
    event_sink_(std::move(ev));
  }
}

void Scheduler::onHandlerReactive(const ReactiveInfo& info) {
  if (user_reactive_callback_) {
    user_reactive_callback_(info);
  }
}

void Scheduler::onHandlerProtocol(const ProtocolInfo& info) {
  // For passive/reactive messages, use session_id=0 and poll_id=0
  // as they are not associated with any scheduled poll item
  bool is_active = (info.message_type == MessageType::active);
  uint32_t s_id =
      is_active ? current_session_id_.load(std::memory_order_acquire) : 0;
  uint16_t p_id =
      is_active ? current_poll_id_.load(std::memory_order_acquire) : 0;

  uint32_t scheduler_attempts = 0;
  if (is_active) {
    // Never block the bus thread here: on contention the attempts count
    // is reported as unknown (reporting-only field, retry decisions use
    // the item counter directly). A blocked bus thread backs up the UART
    // queue, and stale bytes then drive stray arbitration writes.
    platform::UniqueLock<platform::Mutex> lock(data_mutex_, std::try_to_lock);
    if (lock.owns_lock() && active_item_ && active_item_->session_id == s_id) {
      scheduler_attempts = active_item_->item.attempts;
    }
  }

  ProtocolEvent ev;
  ev.type = info.is_error ? ProtocolEvent::Type::error
                          : ProtocolEvent::Type::telegram;
  ev.session_id = s_id;
  ev.poll_id = p_id;
  ev.attempts = scheduler_attempts;
  ev.handler_state = info.handler_state;
  ev.request_state = info.request_state;
  ev.timestamp = ebus::getWallTimeMs();
  ev.level = info.level;
  ev.master.assign(info.master_view.data(), info.master_view.size());
  ev.slave.assign(info.slave_view.data(), info.slave_view.size());
  if (info.is_error) {
    ev.protocol_error = info.protocol_error;
    ev.result = info.result;
    ev.sequence_state = info.sequence_state;
  } else {
    ev.message_type = info.message_type;
    ev.telegram_type = info.telegram_type;
  }

  if (event_sink_) {
    event_sink_(std::move(ev));
  }
}

bool Scheduler::injectProtocolEvent(const ProtocolEvent& event) {
  bool need_reset = false;
  std::optional<ProtocolEvent> arbitration_failure;

  {
    platform::LockGuard<platform::Mutex> lock(data_mutex_);
    if (!active_item_ || event.session_id != active_item_->session_id)
      return false;
    if (event.type == ProtocolEvent::Type::won) return true;
  }

  // Inlined handleAttemptResult logic:
  {
    platform::LockGuard<platform::Mutex> lock(data_mutex_);
    if (!active_item_) return false;
    if (event.type == ProtocolEvent::Type::lost ||
        event.type == ProtocolEvent::Type::error) {
      // Structural protocol errors are not transient; do not retry and do
      // not blame the bus (a bad message is our fault, not the wire's).
      const bool is_fatal =
          (event.type == ProtocolEvent::Type::error &&
           event.protocol_error == ProtocolError::invalid_message);

      // Global TX circuit-breaker: bus-level active failures count,
      // whatever the ZZ target. Trips quarantine ALL active traffic.
      // Two exclusions, same wire-works proof:
      // - Peer silence: error_active_master_ack means our master block
      //   went out clean (echo-verified) but no slave ACK arrived, and
      //   total_transfer_timeout is the scheduler-side catch-all for an
      //   unanswered transfer. The peer is quiet, the bus is not broken.
      // - Lost arbitration: someone else is transmitting, which proves
      //   the wire works as strongly as echo does. Quarantining our
      //   traffic because others talk is self-harm with no protective
      //   value (against a monopolizing jabberer our silence changes
      //   nothing). Scan probes and dead-register polls therefore never
      //   trip the quarantine; echo/corruption faults still do. Retries
      //   and error reporting are unchanged.
      const bool is_peer_silence =
          (event.type == ProtocolEvent::Type::error &&
           (event.protocol_error == ProtocolError::error_active_master_ack ||
            event.protocol_error == ProtocolError::total_transfer_timeout));
      const bool is_lost = (event.type == ProtocolEvent::Type::lost);
      // Cause breakdown for support forensics (which class feeds the
      // breaker). Counted alongside, never reset by outcomes.
      if (handler_) {
        handler_->getMonitor()->updateScheduler([=](auto& m) {
          if (is_fatal)
            m.fail_fatal++;
          else if (is_lost)
            m.fail_lost++;
          else if (is_peer_silence)
            m.fail_silent++;
          else
            m.fail_bus++;
        });
      }
      if (!is_fatal && !is_peer_silence && !is_lost) {
        breaker_consecutive_++;
        if (breaker_consecutive_ >= breaker_threshold_ &&
            !breakerOpenLocked()) {
          breaker_trips_++;
          breaker_open_until_ = Clock::now() + breakerCooldownLocked();
        }
        publishBreakerLocked();
      }

      active_item_->item.attempts++;
      if (!is_fatal && !active_item_->item.cancelled &&
          active_item_->item.attempts < max_attempts_) {
        if (handler_) {
          handler_->getMonitor()->updateScheduler([](auto& m) { m.retries++; });
        }
        // Signal that reset is needed (do it outside mutex)
        need_reset = true;
        // Reschedule with backoff
        active_item_->item.due =
            Clock::now() + backoffDuration(active_item_->item.attempts);
        scheduled_items_.push_back(std::move(active_item_->item));
        std::push_heap(scheduled_items_.begin(), scheduled_items_.end(),
                       Compare());
        active_item_.reset();
        current_session_id_.store(0, std::memory_order_release);
        current_poll_id_.store(0, std::memory_order_release);
        return true;
      }
    }

    auto terminal_item = std::move(active_item_->item);
    if (event.type == ProtocolEvent::Type::lost && !terminal_item.cancelled) {
      // Intermediate losses stay internal. Publish one terminal error when
      // the retry budget is exhausted so clients can observe failed writes.
      arbitration_failure = event;
      arbitration_failure->type = ProtocolEvent::Type::error;
      arbitration_failure->level = LogLevel::error;
      arbitration_failure->protocol_error = ProtocolError::arbitration_lost;
      arbitration_failure->attempts = terminal_item.attempts;
      arbitration_failure->result = RequestResult::first_lost;
      uint8_t master[SequenceLimits::model_capacity]{};
      master[0] = handler_->getSourceAddress();
      const auto length = std::min(terminal_item.message.size(), sizeof(master) - 1);
      std::copy_n(terminal_item.message.begin(), length, master + 1);
      arbitration_failure->master.assign(master, length + 1);
    }
    active_item_.reset();
    current_session_id_.store(0, std::memory_order_release);
    current_poll_id_.store(0, std::memory_order_release);
    if (event.type == ProtocolEvent::Type::telegram &&
        (breaker_consecutive_ > 0 || breakerOpenLocked())) {
      // Any completed active telegram proves the wire is healthy again:
      // close a probe quarantine and restart the failure count AND the
      // cooldown growth, so the next episode (if any) reopens at base
      // instead of inheriting a peak reached days ago. Headless operation
      // depends on this: no user presses re-arm in real life.
      breaker_consecutive_ = 0;
      breaker_trips_ = 0;
      breaker_open_until_ = TimePoint{};
      publishBreakerLocked();
    }
  }

  if (need_reset && handler_) {
    handler_->reset();
  }
  // Dispatch outside the scheduler mutex. The reactor processes this error
  // after the original lost event; it no longer belongs to an active item.
  if (arbitration_failure && event_sink_) {
    event_sink_(std::move(*arbitration_failure));
  }
  return true;
}

bool Scheduler::tick() {
  std::optional<Item> item_to_start;
  ProtocolEvent timeout_ev{};
  bool has_timeout = false;

  {
    platform::LockGuard<platform::Mutex> lock(data_mutex_);
    if (active_item_) {
      auto elapsed = Clock::now() - active_item_->start_time;
      if (elapsed > total_timeout_) {
        timeout_ev.type = ProtocolEvent::Type::error;
        timeout_ev.session_id = active_item_->session_id;
        timeout_ev.poll_id = active_item_->item.poll_id;
        timeout_ev.protocol_error = ProtocolError::total_transfer_timeout;
        timeout_ev.result = RequestResult::first_error;
        timeout_ev.sequence_state = handler_->getActiveSequenceState();
        timeout_ev.level = LogLevel::error;
        timeout_ev.handler_state = handler_->getState();
        timeout_ev.request_state = RequestState::observe;
        timeout_ev.master.assign(active_item_->item.message.data(),
                                 active_item_->item.message.size());
        has_timeout = true;
      }
    } else if (!scheduled_items_.empty() &&
               scheduled_items_.front().due <= Clock::now()) {
      // Breaker quarantine: no new active item starts while open. Expiry
      // needs no explicit close: the next started item is the single probe,
      // its telegram closes the breaker, its failure re-trips it.
      if (breakerOpenLocked()) return false;
      if (handler_->isActiveMessagePending()) return false;
      std::pop_heap(scheduled_items_.begin(), scheduled_items_.end(),
                    Compare());
      item_to_start = std::move(scheduled_items_.back());
      scheduled_items_.pop_back();
      // Keep the active item counted while handing it to the handler. New
      // enqueues must leave room for this item's possible retry.
      active_item_ = {*item_to_start, Clock::now(), item_to_start->session_id};
      current_session_id_.store(item_to_start->session_id,
                                std::memory_order_release);
      current_poll_id_.store(item_to_start->poll_id, std::memory_order_release);
    }
  }

  if (has_timeout) {
    handler_->reset();

    if (event_sink_) {
      event_sink_(std::move(timeout_ev));
    }
    return true;
  }

  if (item_to_start) {
    if (!handler_->sendActiveMessage(item_to_start->message)) {
      ProtocolEvent fail_ev{};
      fail_ev.type = ProtocolEvent::Type::error;
      fail_ev.session_id = item_to_start->session_id;
      fail_ev.poll_id = item_to_start->poll_id;
      fail_ev.protocol_error = ProtocolError::invalid_message;
      fail_ev.result = RequestResult::first_error;
      fail_ev.sequence_state = handler_->getActiveSequenceState();
      fail_ev.level = LogLevel::error;
      fail_ev.handler_state = handler_->getState();
      fail_ev.request_state = RequestState::observe;

      if (event_sink_) {
        event_sink_(std::move(fail_ev));
      }
    }
    return true;
  }
  return false;
}

uint32_t Scheduler::enqueue(uint8_t priority, ByteView message,
                            uint16_t poll_id) {
  Item it;
  it.priority = priority;
  it.due = Clock::now();
  it.message.assign(message);
  const uint32_t session_id = next_session_id_++;
  it.session_id = session_id;
  it.poll_id = poll_id;
  if (pushItem(std::move(it))) return session_id;
  return 0;
}

uint32_t Scheduler::enqueueReplacing(uint8_t priority, ByteView message,
                                     uint32_t replace_session_id) {
  Item item;
  item.priority = priority;
  item.due = Clock::now();
  item.message.assign(message);
  const auto session_id = next_session_id_++;
  item.session_id = session_id;
  if (pushItem(std::move(item), replace_session_id)) return session_id;
  return 0;
}

bool Scheduler::cancel(uint32_t session_id) {
  if (session_id == 0) return false;
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  for (auto it = scheduled_items_.begin(); it != scheduled_items_.end(); ++it) {
    if (it->session_id == session_id) {
      scheduled_items_.erase(it);
      std::make_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
      return true;
    }
  }
  if (active_item_ && active_item_->session_id == session_id) {
    active_item_->item.cancelled = true;
    return true;
  }
  return false;
}

uint32_t Scheduler::enqueueAt(uint8_t priority, ByteView message,
                              TimePoint when, uint16_t poll_id) {
  Item it;
  it.priority = priority;
  it.due = when;
  it.message.assign(message);
  const uint32_t session_id = next_session_id_++;
  it.session_id = session_id;
  it.poll_id = poll_id;
  if (pushItem(std::move(it))) return session_id;
  return 0;
}

void Scheduler::clear() {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  scheduled_items_.clear();
  std::make_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
  active_item_.reset();
}

Clock::time_point Scheduler::nextDueTime() const {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  if (active_item_) {
    return active_item_->start_time + total_timeout_;
  }

  // Quarantined: sleep until the probe time instead of busy-spinning.
  if (breakerOpenLocked()) return breaker_open_until_;

  // Starvation/Busy-wait Fix: If the handler is currently busy (e.g., with an
  // external bridge or reactive response), we cannot start a new transfer.
  // Any pending items should not cause a spin loop in the controller.
  if (handler_ && handler_->isActiveMessagePending()) {
    return Clock::time_point::max();
  }

  if (scheduled_items_.empty()) return Clock::time_point::max();
  return scheduled_items_.front().due;
}

size_t Scheduler::size() const {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  return scheduled_items_.size() + (active_item_ ? 1 : 0);
}

size_t Scheduler::capacity() const { return SchedulerLimits::max_items; }

SchedulerStatus Scheduler::fetchStatus() const {
  return SchedulerStatus{
      QueueStatus("scheduler", size(), capacity(), max_queue_size_)};
}

void Scheduler::resetPeakMetrics() {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  max_queue_size_ = scheduled_items_.size() + (active_item_ ? 1 : 0);
}

bool Scheduler::pushItem(Item&& item, uint32_t replace_session_id) {
  platform::LockGuard<platform::Mutex> lock(data_mutex_);
  auto replaced = scheduled_items_.end();
  if (replace_session_id != 0) {
    replaced = std::find_if(scheduled_items_.begin(), scheduled_items_.end(),
                            [replace_session_id](const Item& queued) {
                              return queued.session_id == replace_session_id;
                            });
  }
  const auto count = scheduled_items_.size() + (active_item_ ? 1 : 0);
  const auto removed = replaced != scheduled_items_.end() ? 1 : 0;
  if (count - removed >= SchedulerLimits::max_items) {
    return false;  // Leave the old session intact if replacement cannot fit.
  }

  if (replaced != scheduled_items_.end()) {
    scheduled_items_.erase(replaced);
    std::make_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
  }
  if (replace_session_id != 0 && active_item_ &&
      active_item_->session_id == replace_session_id) {
    active_item_->item.cancelled = true;
  }
  scheduled_items_.push_back(std::move(item));
  const auto size = scheduled_items_.size() + (active_item_ ? 1 : 0);
  if (size > max_queue_size_) max_queue_size_ = size;
  std::push_heap(scheduled_items_.begin(), scheduled_items_.end(), Compare());
  return true;
}

Scheduler::Duration Scheduler::backoffDuration(
    int attempt) const {  // Pre-calculated multipliers for 2^(attempt-1) to
                          // avoid runtime bit-shifts.
  using Rep = typename Duration::rep;
  static constexpr Rep multipliers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
  constexpr int max_attempt = sizeof(multipliers) / sizeof(multipliers[0]);

  if (attempt <= 0) return Duration::zero();
  if (attempt > max_attempt) return Duration::max();

  Rep factor = multipliers[attempt - 1];
  return Duration(static_cast<Rep>(base_backoff_.count() * factor));
}

std::chrono::milliseconds Scheduler::breakerCooldownLocked() const {
  // Doubling per trip, capped: base, 2*base, 4*base, ... max.
  const uint64_t shift =
      breaker_trips_ == 0 ? 0 : std::min<uint32_t>(breaker_trips_ - 1, 10);
  const uint64_t cooldown_ms =
      static_cast<uint64_t>(breaker_cooldown_base_.count()) << shift;
  const auto max_ms = static_cast<uint64_t>(breaker_cooldown_max_.count());
  return std::chrono::milliseconds(std::min(cooldown_ms, max_ms));
}

void Scheduler::publishBreakerLocked() {
  if (!handler_) return;
  BusMonitor* monitor = handler_->getMonitor();
  if (!monitor) return;
  const bool open = breakerOpenLocked();
  const uint32_t consecutive = breaker_consecutive_;
  const uint32_t trips = breaker_trips_;
  monitor->updateScheduler([=](auto& m) {
    m.consecutive_failures = consecutive;
    m.breaker_trips = trips;
    m.breaker_open = open;
  });
}

}  // namespace ebus::detail