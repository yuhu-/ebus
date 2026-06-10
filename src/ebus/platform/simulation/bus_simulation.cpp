/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if EBUS_SIMULATION
#include "platform/simulation/bus_simulation.hpp"

#include <ebus/protocol_math.hpp>
#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "platform/simulation/virtual_line.hpp"
#include "platform/system.hpp"

namespace ebus::detail::platform {

BusSimulation::BusSimulation(const BusConfig& config,
                             const RuntimeConfig& runtime,
                             detail::Request* request,
                             detail::BusMonitor* monitor)
    : config_(config), runtime_(runtime), request_(request), monitor_(monitor) {
  VirtualLine::get().attach(this);

  syn_base_ms_ = BusLimits::Syn::base_ms;
  syn_tolerance_ms_ = BusLimits::Syn::tolerance_ms;
  syn_address_factor_ms_ = BusLimits::Syn::address_factor_ms;
}

BusSimulation::~BusSimulation() { stop(); }

void BusSimulation::start() {
  if (running_.load(std::memory_order_acquire)) return;

  running_.store(true, std::memory_order_release);
  worker_ = std::make_unique<ServiceThread>(
      "ebus_bus_sim", [this] { simulationReaderLoop(); },
      OrchestrationLimits::bus_stack_size, OrchestrationLimits::bus_priority);
  worker_->start();

  if (runtime_.bus.syn_gen) {
    syn_running_.store(true);
    syn_worker_ = std::make_unique<ServiceThread>(
        "ebus_bus_sim_syn", [this] { simulationSynLoop(); },
        OrchestrationLimits::bus_syn_stack_size,
        OrchestrationLimits::bus_syn_priority);
    syn_worker_->start();
  }
}

void BusSimulation::stop() {
  if (!running_.load(std::memory_order_acquire)) return;
  running_.store(false, std::memory_order_release);
  syn_running_.store(false);

  VirtualLine::get().detach(this);
  {
    platform::UniqueLock<platform::Mutex> lock(syn_mutex_);
    syn_cv_.notify_all();
  }

  if (syn_worker_) syn_worker_->join();
  if (worker_) worker_->join();
}

void BusSimulation::setWindow(const uint16_t window_us) {
  // Not directly used in simulation, but kept for API compatibility
  runtime_.bus.window_us = window_us;
}

void BusSimulation::setOffset(const uint16_t offset_us) {
  // Not directly used in simulation, but kept for API compatibility
  runtime_.bus.offset_us = offset_us;
}

void BusSimulation::setRuntimeConfig(const RuntimeConfig& runtime) {
  bool should_start_syn = false;
  bool should_stop_syn = false;

  {
    platform::LockGuard<platform::Mutex> lock(syn_mutex_);
    bool was_syn_gen_enabled = runtime_.bus.syn_gen;
    runtime_ = runtime;  // Update runtime config

    // Recalculate SYN timing parameters based on new runtime config
    syn_base_ms_ = BusLimits::Syn::base_ms;
    syn_tolerance_ms_ = BusLimits::Syn::tolerance_ms;
    syn_address_factor_ms_ = BusLimits::Syn::address_factor_ms;

    if (runtime_.bus.syn_gen && !was_syn_gen_enabled) {
      should_start_syn = true;
    } else if (!runtime_.bus.syn_gen && was_syn_gen_enabled) {
      should_stop_syn = true;
    }

    // If SYN generator is active, update its next expiry based on new config
    if (syn_running_.load() && !should_stop_syn) {
      resetSynTimerSimInternal(
          Symbols::syn);  // Force re-evaluation of next_syn_expiry_
    }
  }

  if (should_start_syn) {
    syn_running_.store(true);
    syn_worker_ = std::make_unique<ServiceThread>(
        "ebus_bus_sim_syn", [this] { simulationSynLoop(); },
        OrchestrationLimits::bus_syn_stack_size,
        OrchestrationLimits::bus_syn_priority);
    syn_worker_->start();
  } else if (should_stop_syn) {
    syn_running_.store(false);
    syn_cv_.notify_all();
    if (syn_worker_) syn_worker_->join();
    syn_worker_.reset();
  }
}

void BusSimulation::writeByte(const uint8_t byte) {
  lockAndInvoke(listeners_mutex_, getWriteListeners(), byte);

  if (monitor_) monitor_->transmit.markBegin();

  if (byte != Symbols::syn) {
    platform::LockGuard<platform::Mutex> lock(syn_mutex_);
    syn_active_ = false;
  }
  // 1. Simulate the time it takes for the UART to shift the bits out
  // 10 bits (Start + 8 Data + Stop) at 2400 baud
  uint32_t total_delay_us = static_cast<uint32_t>(10 * Physical::bit_time_us);

#if defined(ESP_PLATFORM)
  // Optimization: Split the delay into yielding milliseconds and a short
  // busy-wait. This prevents CPU starvation on single-core ESP32-C3 while
  // maintaining microsecond precision.
  if (total_delay_us >= 1000) platform::sleepMilli(total_delay_us / 1000);
  if (total_delay_us % 1000 > 0) platform::sleepMicro(total_delay_us % 1000);
#else
  platform::sleepMicro(total_delay_us);
#endif

  // 2. Only now does the byte actually appear on the "Wire"
  VirtualLine::get().write(byte);

  if (monitor_) monitor_->transmit.markEnd();
}

void BusSimulation::recordUtilization(uint8_t byte) {
  // 1 (start bit) + zero bits in data.
  if (monitor_) monitor_->recordLowBits(countZeroBits(byte) + 1);
}

ServiceThread::Status BusSimulation::getThreadStatus() const {
  if (worker_) {
    return worker_->status();
  }
  return ServiceThread::Status{"ebus_bus_sim", -1, -1};
}

ServiceThread::Status BusSimulation::getSynThreadStatus() const {
  if (syn_worker_) {
    return syn_worker_->status();
  }
  return ServiceThread::Status{"ebus_bus_sim_syn", -1, -1};
}

ebus::BusStatus BusSimulation::getStatus() const {
  auto map =
      [](const platform::ServiceThread::Status& s) -> ebus::ThreadStatus {
    return {s.name, s.task_stack_bytes, s.task_stack_free_bytes};
  };
  return {map(getThreadStatus()), map(getSynThreadStatus())};
}

void BusSimulation::simulationReaderLoop() {
  uint8_t byte;
  while (running_.load()) {
    if (VirtualLine::get().read(
            this, byte, BusLimits::platform::Posix::virtual_read_timeout_ms)) {
      auto arrival_time = Clock::now();

      lockAndInvoke(listeners_mutex_, getReadListeners(), byte);
      recordUtilization(byte);
      resetSynTimerSim(byte);

      BusEvent event;
      event.byte = byte;
      event.timestamp = arrival_time;
      event.bus_request =
          bus_request_flag_.exchange(false, std::memory_order_acq_rel);
      event.start_bit = false;  // Not applicable in simulation
      lockAndInvoke(listeners_mutex_, getBusEventListeners(), event);

      // --- CRITICAL POINT: Spec 6.3 Immediate Bus Access ---
      // We check for arbitration intent AFTER notifying software to ensure
      // the state machine can pre-load the intent for the NEXT syn, while
      // hardware acts on the CURRENT syn.
      if (byte == Symbols::syn && request_->busRequestPending()) {
        // Yield before busy-waiting to allow lower priority tasks to run
        platform::sleepMilli(1);
        sleepMicro(BusLimits::platform::Posix::request_delay_us);
        writeByte(request_->busRequestAddress());
        bus_request_flag_.store(true, std::memory_order_release);
      }
      // Add a small delay to yield CPU to other tasks, especially lower
      // priority ones.
      platform::sleepMilli(1);  // Yield to other tasks
    } else {
      platform::sleepMilli(1);  // Yield to other tasks if no data is available
    }
  }
}

void BusSimulation::resetSynTimerSim(uint8_t byte) {
  platform::LockGuard<platform::Mutex> lock(syn_mutex_);
  resetSynTimerSimInternal(byte);
}

void BusSimulation::resetSynTimerSimInternal(uint8_t byte) {
  auto now = Clock::now();
  last_activity_time_ = now;

  auto current_t_unique =
      std::chrono::milliseconds(syn_base_ms_) +
      std::chrono::milliseconds(runtime_.address * syn_address_factor_ms_) +
      std::chrono::milliseconds(syn_tolerance_ms_);

  if (syn_active_ && byte == Symbols::syn) {
    next_syn_expiry_ = now + std::chrono::milliseconds(syn_base_ms_);
  } else {
    syn_active_ = false;
    next_syn_expiry_ = now + current_t_unique;
  }
  syn_cv_.notify_one();
}

void BusSimulation::simulationSynLoop() {
  while (syn_running_.load()) {
    platform::UniqueLock<platform::Mutex> lock(syn_mutex_);
    auto now = Clock::now();

    auto current_t_unique =
        std::chrono::milliseconds(syn_base_ms_) +
        std::chrono::milliseconds(runtime_.address * syn_address_factor_ms_) +
        std::chrono::milliseconds(syn_tolerance_ms_);

    if (next_syn_expiry_ ==
        Clock::time_point{}) {  // First run or after stop/start
      next_syn_expiry_ = now + current_t_unique;
    }

    if (next_syn_expiry_ > now) {
      syn_cv_.wait_until(lock, next_syn_expiry_);
      continue;
    }

    // Carrier Sense
    if (now - last_activity_time_ <
        std::chrono::milliseconds(BusLimits::Syn::carrier_sense_ms)) {
      if (monitor_)
        monitor_->updateBus([](auto& m) { m.syn_postponed_count++; });
      if (syn_intent_time_sim_ == Clock::time_point{})
        syn_intent_time_sim_ = now;
      next_syn_expiry_ =
          now + std::chrono::milliseconds(BusLimits::Syn::postpone_ms);
      continue;
    }

    if (syn_intent_time_sim_ != Clock::time_point{} && monitor_) {
      monitor_->syn_postpone.addSample(static_cast<float>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              now - syn_intent_time_sim_)
              .count()));
      syn_intent_time_sim_ = {};
    }

    // We are about to generate a SYN, mark ourselves as active
    syn_active_ = true;
    lock.unlock();

    lockAndInvoke(listeners_mutex_, getSynListeners());

    writeByte(Symbols::syn);

    lock.lock();
    // Safety Fallback:
    // If the timer hasn't been updated by readerThread (receiving the echo),
    // reset it to the unique (long) value as a fallback.
    // This handles cases where our write failed or the echo was corrupted.
    if (next_syn_expiry_ <= Clock::now()) {
      next_syn_expiry_ = Clock::now() + current_t_unique;
    }
  }
}

}  // namespace ebus::detail::platform

#endif  // EBUS_SIMULATION