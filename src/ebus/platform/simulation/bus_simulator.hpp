/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if EBUS_SIMULATION
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <condition_variable>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <ebus/virtual_bus.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/telegram.hpp"
#include "platform/queue.hpp"
#include "platform/service_thread.hpp"
#include "platform/simulation/virtual_line.hpp"
#include "platform/simulation/bus_simulation.hpp"
#include "platform/system.hpp"
#include "utils/circular_buffer.hpp"

namespace ebus::detail {

/**
 * Automated responder for Bus simulation. This is an internal utility.
 */
class BusSimulator {
 public:
  // Lifecycle
  explicit BusSimulator(platform::BusSimulation& bus)
      : bus_(bus), outbound_queue_(16) {
    bus_.addReadListener([this](uint8_t b) { this->onRead(b); });
    worker_ = std::make_unique<platform::ServiceThread>(
        "ebus_sim_worker", [this] { processResponses(); },
        OrchestrationLimits::default_stack_size,
        OrchestrationLimits::default_priority);
    worker_->start();
  }
  ~BusSimulator() { stop(); }
  void stop() {
    outbound_queue_.shutdown();
    if (worker_) worker_->join();
  }

  // Working Methods
  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    reactions_.clear();
    write_history_.clear();
    outbound_queue_.clear();
  }

  uint32_t addMockReaction(VirtualBus::MockReaction reaction) {
    std::lock_guard<std::mutex> lock(mtx_);
    reaction.id = ++next_reaction_id_;
    reactions_.push_back(std::move(reaction));
    return reaction.id;
  }

  void removeMockReaction(uint32_t id) {
    std::lock_guard<std::mutex> lock(mtx_);
    reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                    [id](const auto& r) { return r.id == id; }),
                     reactions_.end());
  }

  void removeMockReaction(const ebus::Sequence& trigger) {
    std::lock_guard<std::mutex> lock(mtx_);
    reactions_.erase(std::remove_if(reactions_.begin(), reactions_.end(),
                                    [&](const auto& r) {
                                      return r.trigger.logicallyEquals(trigger);
                                    }),
                     reactions_.end());
  }

  /**
   * @brief Injects a master message.
   * This remains synchronous because it simulates an external participant
   * and is usually called from test setup, not from inside a bus listener.
   */
  void injectMasterMessage(uint8_t source, ebus::ByteView payload) {
    auto msg = ebus::frameMaster(source, payload);
    for (uint8_t b : msg) {
      bus_.writeByte(b);
    }
  }

  /**
   * @brief Injects a single raw byte.
   */
  void injectRawByte(uint8_t byte) { bus_.writeByte(byte); }

  /**
   * @brief Simulates a physical collision on the wire.
   */
  void injectCollision(uint8_t byte1, uint8_t byte2) {
    platform::VirtualLine::get().writeCollision(byte1, byte2);
  }

  /**
   * @brief Injects a complete master-slave message exchange onto the bus.
   * Master message -> Master ACK -> Slave message -> Slave ACK -> SYN.
   * @param source The source address (QQ).
   * @param master_payload The master payload bytes (ZZ through DBx).
   * @param slave_payload The slave payload bytes (NN DBx).
   */
  void injectMasterSlaveMessage(uint8_t source, ebus::ByteView master_payload,
                                ebus::ByteView slave_payload) {
    auto master_msg = ebus::frameMaster(source, master_payload);
    for (uint8_t b : master_msg) bus_.writeByte(b);
    bus_.writeByte(ebus::Symbols::ack);

    auto slave_msg = ebus::frameSlave(slave_payload);
    for (uint8_t b : slave_msg) bus_.writeByte(b);
    bus_.writeByte(ebus::Symbols::ack);
    bus_.writeByte(ebus::Symbols::syn);
  }

 private:
  platform::BusSimulation& bus_;
  std::mutex mtx_;
  uint32_t next_reaction_id_ = 0;

  struct ResponseItem {
    uint8_t data[SequenceLimits::default_capacity];
    uint8_t len = 0;
    uint32_t delay_ms = 0;  // Added delay
  };

  CircularBuffer<uint8_t, SequenceLimits::default_capacity> write_history_;
  std::vector<VirtualBus::MockReaction> reactions_;
  platform::Queue<ResponseItem> outbound_queue_;
  std::unique_ptr<platform::ServiceThread> worker_;

  // Renamed from onWrite to onRead to reflect that it observes the bus
  void onRead(uint8_t b) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_history_.push_back(b);
    for (auto& reaction : reactions_) {
      if (reaction.repeat_count == -1) continue;  // Disabled, skip

      // Check if the current history suffix matches the trigger pattern
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

  void processResponses() {
    ResponseItem item;
    // Blocking pop handles the sleep/wait automatically
    while (outbound_queue_.pop(item)) {
      if (item.delay_ms > 0) {
        platform::sleepMilli(item.delay_ms);
      }
      for (size_t i = 0; i < item.len; ++i) {
        bus_.writeByte(item.data[i]);
      }
    }
  }
};

}  // namespace ebus::detail

#endif  // EBUS_SIMULATION
