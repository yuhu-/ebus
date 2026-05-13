/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if defined(POSIX)
#include "platform/posix/bus_posix.hpp"

#include <algorithm>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/protocol_math.hpp>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "platform/system.hpp"

namespace ebus::detail::platform {

BusPosix::BusPosix(const BusConfig& config, const ebus::RuntimeConfig& runtime,
                   Request* request, BusMonitor* monitor)
    : config_(config),
      runtime_(runtime),
      request_(request),
      monitor_(monitor),
      fd_(-1),
      open_(false),
      byte_queue_(std::make_unique<Queue<BusEvent>>(BusLimits::queue_size)),
      worker_(),
      running_(false),  // Initialize running_ before syn_worker_
      syn_worker_() {}  // Initialize syn_worker_ after running_

BusPosix::~BusPosix() { stop(); }

void BusPosix::start() {
  if (open_) return;

  if (config_.simulate) {
#if EBUS_SIMULATION_ENABLED
    VirtualLine::get().attach(this);
    fd_ = -1;
    open_ = true;
#else
    throw std::runtime_error("Simulation mode is not enabled in this build.");
#endif
  } else {
    struct termios new_settings;
    fd_ = ::open(config_.device.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0 || isatty(fd_) == 0)
      throw std::runtime_error("Failed to open ebus device: " + config_.device);

    tcgetattr(fd_, &old_settings_);
    ::memset(&new_settings, 0, sizeof(new_settings));
    new_settings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
    new_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    new_settings.c_iflag |= IGNPAR;
    new_settings.c_oflag &= ~OPOST;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;

    tcflush(fd_, TCIFLUSH);
    tcsetattr(fd_, TCSAFLUSH, &new_settings);
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) & ~O_NONBLOCK);

    open_ = true;
  }

  running_.store(true);
  worker_ = std::make_unique<ServiceThread>(
      "ebus_bus", [this] { readerThread(); },
      detail::OrchestrationLimits::bus_stack_size,
      detail::OrchestrationLimits::bus_priority);
  worker_->start();

  // start SYN generator if enabled in config
  if (monitor_) monitor_->uptime.markBegin();
  if (runtime_.bus.syn_gen) {
    syn_base_ms_dur_ = std::chrono::milliseconds(BusLimits::Syn::base_ms);
    syn_tolerance_ms_dur_ =
        std::chrono::milliseconds(BusLimits::Syn::tolerance_ms);
    current_t_unique_ =
        syn_base_ms_dur_ +
        std::chrono::milliseconds(runtime_.address *
                                  BusLimits::Syn::address_factor_ms) +
        syn_tolerance_ms_dur_;
    next_syn_expiry_ = Clock::now() + current_t_unique_;

    syn_active_ = false;
    syn_running_.store(true);
    syn_worker_ = std::make_unique<ServiceThread>(
        "ebus_bus_syn", [this] { synThread(); },
        detail::OrchestrationLimits::bus_syn_stack_size,
        detail::OrchestrationLimits::bus_syn_priority);
    syn_worker_->start();
  }
}

void BusPosix::stop() {
  if (!open_) return;
  if (monitor_) monitor_->uptime.markEnd();

  running_.store(false);
  syn_running_.store(false);

  if (byte_queue_) byte_queue_->shutdown();
#if EBUS_SIMULATION_ENABLED
  if (config_.simulate) VirtualLine::get().detach(this);
#endif

  {
    std::unique_lock<std::mutex> lock(syn_mutex_);
    syn_cv_.notify_all();
  }

  if (!config_.simulate && fd_ != -1) ::tcflush(fd_, TCIOFLUSH);

  if (syn_worker_) syn_worker_->join();
  if (worker_) worker_->join();

  if (!config_.simulate && fd_ != -1) {
    ::tcsetattr(fd_, TCSANOW, &old_settings_);
    ::close(fd_);
  }

  fd_ = -1;
  open_ = false;
}

Queue<BusEvent>* BusPosix::getQueue() const { return byte_queue_.get(); }

void BusPosix::writeByte(const uint8_t byte) {
  {
    std::lock_guard<std::mutex> l_lock(listeners_mutex_);
    std::lock_guard<std::mutex> lock(syn_mutex_);
    auto now = Clock::now();
    last_activity_time_ = now;
    // Postpone automated SYN generation. Add 4ms to account for serialization.
    if (byte != Symbols::syn) {
      syn_active_ = false;
    }
    next_syn_expiry_ =
        now + current_t_unique_ +
        std::chrono::milliseconds(BusLimits::Syn::serialization_delay_ms);
    syn_cv_.notify_one();
  }

  if (monitor_) monitor_->transmit.markBegin();

  if (config_.simulate) {
#if EBUS_SIMULATION_ENABLED
    // Notify write listeners for local simulation feedback
    for (const auto& listener : write_listeners_) listener(byte);
    VirtualLine::get().write(byte);
#endif
  } else {
    ensureOpen();
    {
      std::lock_guard<std::mutex> lock(listeners_mutex_);
      for (const auto& listener : write_listeners_) listener(byte);
    }
    if (::write(fd_, &byte, 1) == -1)
      throw std::runtime_error("BusPosix: write error");
  }
  if (monitor_) monitor_->transmit.markEnd();
}

void BusPosix::setWindow(const uint16_t window_us) {
  // Validate window
  runtime_.bus.window_us = (window_us < BusLimits::window_min_us ||
                            window_us > BusLimits::window_max_us)
                               ? ebus::RuntimeConfig{}.bus.window_us
                               : window_us;
}

void BusPosix::setOffset(const uint16_t offset_us) {
  // Validate offset
  runtime_.bus.offset_us = (offset_us > BusLimits::offset_max_us)
                               ? ebus::RuntimeConfig{}.bus.offset_us
                               : offset_us;
}

void BusPosix::setRuntimeConfig(const RuntimeConfig& runtime) {
  bool should_start = false;
  bool should_stop = false;

  {
    std::lock_guard<std::mutex> lock(syn_mutex_);
    bool was_enabled = runtime_.bus.syn_gen;
    runtime_ = runtime;

    // Validate window and offset
    if (runtime_.bus.window_us < BusLimits::window_min_us ||
        runtime_.bus.window_us > BusLimits::window_max_us)
      runtime_.bus.window_us = ebus::RuntimeConfig{}.bus.window_us;
    if (runtime_.bus.offset_us > BusLimits::offset_max_us)
      runtime_.bus.offset_us = ebus::RuntimeConfig{}.bus.offset_us;

    // Always recalculate timing durations based on the new configuration
    syn_base_ms_dur_ = std::chrono::milliseconds(BusLimits::Syn::base_ms);
    syn_tolerance_ms_dur_ =
        std::chrono::milliseconds(BusLimits::Syn::tolerance_ms);
    current_t_unique_ =
        syn_base_ms_dur_ +
        std::chrono::milliseconds(runtime_.address *
                                  BusLimits::Syn::address_factor_ms) +
        syn_tolerance_ms_dur_;

    // Manage thread transitions only if the bus is currently active
    if (open_ && running_.load()) {
      if (runtime_.bus.syn_gen && !was_enabled && !syn_running_.load()) {
        should_start = true;
        syn_running_.store(true);
        next_syn_expiry_ = Clock::now() + current_t_unique_;
        syn_active_ = false;
      } else if (!runtime_.bus.syn_gen && was_enabled && syn_running_.load()) {
        should_stop = true;
        syn_running_.store(false);
        syn_cv_.notify_all();
      }

      // If generator was already running, re-align next_syn_expiry_ to the new
      // t_unique
      if (runtime_.bus.syn_gen && !should_start && syn_running_.load()) {
        next_syn_expiry_ = Clock::now() + current_t_unique_;
      }
    }
  }

  // Execute thread lifecycle actions outside of the lock to prevent deadlocks
  if (should_start) {
    if (syn_worker_) syn_worker_->join();  // Join existing thread if any
    syn_worker_ = std::make_unique<ServiceThread>(  // Create new ServiceThread
        "ebus_bus_syn", [this] { synThread(); },
        detail::OrchestrationLimits::bus_syn_stack_size,
        detail::OrchestrationLimits::bus_syn_priority);
    syn_worker_->start();  // Start the new ServiceThread
  } else if (should_stop) {
    if (syn_worker_) syn_worker_->join();  // Join existing thread if any
    syn_worker_.reset();                   // Release the unique_ptr
  }
}

void BusPosix::addReadListener(ReadListener listener) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  read_listeners_.push_back(listener);
}

void BusPosix::addWriteListener(WriteListener listener) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  write_listeners_.push_back(listener);
}

void BusPosix::addSynListener(SynListener listener) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  syn_listeners_.push_back(listener);
}

ServiceThread::Status BusPosix::getThreadStatus() const {
  if (worker_) {
    return worker_->status();
  }
  return ServiceThread::Status{"ebus_bus", -1, -1};
}

ServiceThread::Status BusPosix::getSynThreadStatus() const {
  if (syn_worker_) {
    return syn_worker_->status();
  }
  return ServiceThread::Status{"ebus_bus_syn", -1, -1};
}

ebus::BusStatus BusPosix::getStatus() const {
  auto map =
      [](const platform::ServiceThread::Status& s) -> ebus::ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };
  ebus::BusStatus s;
  s.bus_thread = map(getThreadStatus());
  s.syn_thread = map(getSynThreadStatus());
  return s;
}

void BusPosix::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data. eBUS bit time is ~416.67us
  float low_time = (countZeroBits(byte) + 1) * detail::Physical::bit_time_us;
  if (monitor_) monitor_->utilization.addSample(low_time);
}

void BusPosix::ensureOpen() const {
  if (!open_ || fd_ < 0) throw std::runtime_error("BusPosix: device not open");
}

void BusPosix::readerThread() {
  while (running_.load()) {
    uint8_t byte;
    ssize_t n = 0;

    if (config_.simulate) {
#if EBUS_SIMULATION_ENABLED
      // Use the memory-based simulation queue
      if (VirtualLine::get().read(
              this, byte, BusLimits::platform::Posix::virtual_read_timeout_ms))
        n = 1;
      else
        continue;  // timeout, check running_ flag again
#endif
    } else {
      n = ::read(fd_, &byte, 1);
    }

    if (n == 1) {
      auto arrival_time = Clock::now();
      {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        for (const auto& listener : read_listeners_) listener(byte);
      }

      recordUtilization(byte);

      // Notify SYN generator that a symbol was recognised (end of char)
      resetSynTimer(byte);

      detail::BusEvent event;
      event.byte = byte;
      event.bus_request =
          bus_request_flag_.exchange(false, std::memory_order_acq_rel);
      event.start_bit = false;
      // In POSIX, we don't have ISR-level start bit detection like ESP32.
      // If a framing error occurs, it's a strong indicator of a start bit
      // issue. For now, we'll increment this counter in the BusFreeRtos only.
      // If POSIX serial drivers provide more granular error types, we could map
      // them here.
      // if (monitor_) monitor_->updateBus([](auto& m){ m.start_bit_errors++;
      // });
      event.timestamp = arrival_time;

      if (byte_queue_) byte_queue_->push(event);

      // Hit the 4300-4456us window (approx 200us after SYN reception)
      if (byte == Symbols::syn && request_->busRequestPending()) {
        sleepMicro(BusLimits::platform::Posix::request_delay_us);
        writeByte(request_->busRequestAddress());
        bus_request_flag_.store(true, std::memory_order_release);
      }
    } else if (n == 0) {
      // EOF - stop thread
      break;
    } else {
      // read error, optionally break or continue after short sleep
      if (errno == EINTR) continue;
      break;
    }
  }
  running_.store(false);
}

void BusPosix::resetSynTimer(uint8_t byte) {
  std::lock_guard<std::mutex> lock(syn_mutex_);
  auto now = Clock::now();
  last_activity_time_ = now;

  // Arbitration Logic:
  // If we are the active SYN generator (synActive_ is true) and we receive
  // the echo of our own SYN (byte == syn), we "won" arbitration or the bus
  // is idle. We continue generating SYNs at the fast rate (syn_base_ms_dur_).
  if (syn_active_ && byte == Symbols::syn) {
    next_syn_expiry_ = now + syn_base_ms_dur_;
  } else {
    syn_active_ = false;
    next_syn_expiry_ = now + current_t_unique_;
  }

  syn_cv_.notify_one();
}

void BusPosix::synThread() {
  while (syn_running_.load()) {
    std::unique_lock<std::mutex> lock(syn_mutex_);

    auto now = Clock::now();
    if (next_syn_expiry_ > now) {
      syn_cv_.wait_until(lock, next_syn_expiry_);
      continue;
    }

    // Carrier Sense: If the bus was active very recently (e.g. a write
    // started), postpone generation to avoid colliding with the byte being
    // serialized.
    if (now - last_activity_time_ <
        std::chrono::milliseconds(BusLimits::Syn::carrier_sense_ms)) {
      if (monitor_)
        monitor_->updateBus([](auto& m) { m.syn_postponed_count++; });
      if (syn_intent_time_ == Clock::time_point{}) syn_intent_time_ = now;
      next_syn_expiry_ =
          now + std::chrono::milliseconds(BusLimits::Syn::postpone_ms);
      continue;
    }

    if (syn_intent_time_ != Clock::time_point{} && monitor_) {
      monitor_->syn_postpone.addSample(static_cast<float>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              now - syn_intent_time_)
              .count()));
      syn_intent_time_ = {};
    }

    // We are about to generate a SYN, mark ourselves as active
    syn_active_ = true;
    lock.unlock();

    {
      std::lock_guard<std::mutex> l_lock(listeners_mutex_);
      for (const auto& listener : syn_listeners_) listener();
    }
    writeByte(Symbols::syn);

    lock.lock();
    // Safety Fallback:
    // If the timer hasn't been updated by readerThread (receiving the echo),
    // reset it to the unique (long) value as a fallback.
    // This handles cases where our write failed or the echo was corrupted.
    if (next_syn_expiry_ <= Clock::now()) {
      next_syn_expiry_ = Clock::now() + current_t_unique_;
    }
  }
}

}  // namespace ebus::detail::platform

#endif  // POSIX
