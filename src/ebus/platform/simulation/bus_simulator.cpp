/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if EBUS_SIMULATION
#include "platform/simulation/bus_simulator.hpp"

#include <algorithm>
#include <cstring>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/protocol_math.hpp>
#include <ebus/utils.hpp>
#include <memory>
#include <vector>

#include "platform/service_thread.hpp"
#include "platform/simulation/bus_simulation.hpp"
#include "platform/simulation/bus_simulator.hpp"
#include "platform/simulation/virtual_line.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

BusSimulator::BusSimulator(platform::BusSimulation& bus)
    : bus_(bus), outbound_queue_(16) {
  bus_.addReadListener(
      platform::Delegate<void(const uint8_t&)>::bind<BusSimulator, &BusSimulator::onRead>(this));
  worker_ = std::make_unique<platform::ServiceThread>(
      "ebus_sim_worker", [this] { processResponses(); },
      OrchestrationLimits::default_stack_size,
      OrchestrationLimits::default_priority);
  worker_->start();
}

BusSimulator::~BusSimulator() { stop(); }

void BusSimulator::stop() {
  outbound_queue_.shutdown();
  if (worker_) worker_->join();
}

void BusSimulator::clear() {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  reactions_.clear();
  write_history_.clear();
  outbound_queue_.clear();
}

uint32_t BusSimulator::addMockReaction(VirtualBus::MockReaction reaction) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  uint32_t new_id = ++next_reaction_id_;
  reaction.id = new_id;
  reactions_.push_back(std::move(reaction));
  return new_id;
}

void BusSimulator::removeMockReaction(uint32_t id) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                  [id](const auto& r) { return r.id == id; }),
                   reactions_.end());
}

void BusSimulator::removeMockReaction(const ebus::Sequence& trigger) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                  [&](const auto& r) {
                                    return r.trigger.logicallyEquals(trigger);
                                  }),
                   reactions_.end());
}

void BusSimulator::injectRawByte(uint8_t byte) {
  ResponseItem item;
  item.data[0] = byte;
  item.len = 1;
  outbound_queue_.push(item);
}

void BusSimulator::injectCollision(uint8_t byte1, uint8_t byte2) {
  platform::VirtualLine::get().writeCollision(byte1, byte2);
}

void BusSimulator::injectMasterMessage(uint8_t source, ebus::ByteView payload) {
  auto msg = ebus::frameMaster(source, payload);
  ResponseItem item;
  item.len = static_cast<uint8_t>(std::min(msg.size(), sizeof(item.data)));
  std::memcpy(item.data, msg.data(), item.len);
  item.wait_for_syn = true;
  item.delay_us = BusLimits::platform::Posix::request_delay_us;
  outbound_queue_.push(item);
}

void BusSimulator::injectMasterSlaveMessage(uint8_t source,
                                            ebus::ByteView master_payload,
                                            ebus::ByteView slave_payload) {
  ebus::Sequence full;
  full.append(ebus::frameMaster(source, master_payload));
  full.pushBack(ebus::Symbols::ack, false);
  full.append(ebus::frameSlave(slave_payload));
  full.pushBack(ebus::Symbols::ack, false);
  full.pushBack(ebus::Symbols::syn, false);

  ResponseItem item;
  item.len = static_cast<uint8_t>(std::min(full.size(), sizeof(item.data)));
  std::memcpy(item.data, full.data(), item.len);
  item.wait_for_syn = true;
  item.delay_us = BusLimits::platform::Posix::request_delay_us;
  outbound_queue_.push(item);
}

void BusSimulator::onRead(const uint8_t& b) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (b == ebus::Symbols::syn) {
    syn_received_ = true;
    syn_cv_.notify_all();
  }
  write_history_.push_back(b);
  for (auto& reaction : reactions_) {
    if (reaction.repeat_count == -1) continue;

    bool match = false;
    if (write_history_.size() >= reaction.trigger.size()) {
      match = ebus::matches(write_history_, reaction.trigger,
                            write_history_.size() - reaction.trigger.size());
    }

    if (match) {
      ResponseItem item;
      item.delay_ms = reaction.delay_ms;
      item.len = static_cast<uint8_t>(
          std::min(reaction.action.size(), sizeof(item.data)));
      std::memcpy(item.data, reaction.action.data(), item.len);
      if (outbound_queue_.tryPush(item)) {
        if (reaction.repeat_count > 0) {
          reaction.repeat_count--;
          if (reaction.repeat_count == 0) reaction.repeat_count = -1;
        }
        return;
      }
    }
  }
}

void BusSimulator::processResponses() {
  ResponseItem item;
  while (outbound_queue_.pop(item)) {
    if (item.wait_for_syn) {
      platform::UniqueLock<platform::Mutex> lock(mutex_);
      syn_received_ = false;
      syn_cv_.wait(lock, [this] { return syn_received_ || outbound_queue_.isShutdown(); });
      if (outbound_queue_.isShutdown()) return;
    }

    if (item.delay_ms > 0) {
      platform::sleepMilli(item.delay_ms);
    }
    if (item.delay_us > 0) {
      platform::sleepMicro(item.delay_us);
    }
    for (size_t i = 0; i < item.len; ++i) {
      bus_.writeByte(item.data[i]);
    }
  }
}

}  // namespace ebus::detail

#endif
