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
#include <mutex>
#include <vector>

#include "platform/service_thread.hpp"
#include "platform/simulation/bus_simulation.hpp"
#include "platform/simulation/bus_simulator.hpp"
#include "platform/simulation/virtual_line.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

BusSimulator::BusSimulator(platform::BusSimulation& bus)
    : bus_(bus), outbound_queue_(16) {
  bus_.addReadListener([this](uint8_t b) { this->onRead(b); });
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
  std::lock_guard<std::mutex> lock(mtx_);
  reactions_.clear();
  write_history_.clear();
  outbound_queue_.clear();
}

uint32_t BusSimulator::addMockReaction(VirtualBus::MockReaction reaction) {
  std::lock_guard<std::mutex> lock(mtx_);
  reaction.id = ++next_reaction_id_;
  reactions_.push_back(std::move(reaction));
  return reaction.id;
}

void BusSimulator::removeMockReaction(uint32_t id) {
  std::lock_guard<std::mutex> lock(mtx_);
  reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                  [id](const auto& r) { return r.id == id; }),
                   reactions_.end());
}

void BusSimulator::removeMockReaction(const ebus::Sequence& trigger) {
  std::lock_guard<std::mutex> lock(mtx_);
  reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                  [&](const auto& r) {
                                    return r.trigger.logicallyEquals(trigger);
                                  }),
                   reactions_.end());
}

void BusSimulator::injectRawByte(uint8_t byte) { bus_.writeByte(byte); }

void BusSimulator::injectCollision(uint8_t byte1, uint8_t byte2) {
  platform::VirtualLine::get().writeCollision(byte1, byte2);
}

void BusSimulator::injectMasterMessage(uint8_t source, ebus::ByteView payload) {
  auto msg = ebus::frameMaster(source, payload);
  for (uint8_t b : msg) {
    bus_.writeByte(b);
  }
}

void BusSimulator::injectMasterSlaveMessage(uint8_t source,
                                            ebus::ByteView master_payload,
                                            ebus::ByteView slave_payload) {
  auto master_msg = ebus::frameMaster(source, master_payload);
  for (uint8_t b : master_msg) bus_.writeByte(b);
  bus_.writeByte(ebus::Symbols::ack);

  auto slave_msg = ebus::frameSlave(slave_payload);
  for (uint8_t b : slave_msg) bus_.writeByte(b);
  bus_.writeByte(ebus::Symbols::ack);
  bus_.writeByte(ebus::Symbols::syn);
}

void BusSimulator::onRead(uint8_t b) {
  std::lock_guard<std::mutex> lock(mtx_);
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
    if (item.delay_ms > 0) {
      platform::sleepMilli(item.delay_ms);
    }
    for (size_t i = 0; i < item.len; ++i) {
      bus_.writeByte(item.data[i]);
    }
  }
}

}  // namespace ebus::detail

#endif
