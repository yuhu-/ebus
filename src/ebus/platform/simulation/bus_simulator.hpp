/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if EBUS_SIMULATION
#include <atomic>
#include <chrono>
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
#include "platform/simulation/bus_simulation.hpp"
#include "platform/system.hpp"
#include "utils/circular_buffer.hpp"

namespace ebus::detail {

/**
 * Automated responder for Bus simulation. This is an internal utility.
 */
class BusSimulator {
 public:
  /**
   * @brief Configuration for an automated mock action.
   */
  struct MockReaction {
    Sequence trigger;       ///< Sequence of bytes that triggers the mock.
    Sequence action;        ///< Bytes to inject when triggered.
    int repeat_count = 1;   ///< 0 for infinite, -1 for disabled, > 0 finite.
    uint32_t delay_ms = 0;  ///< Delay before injection.
  };

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

  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    reactions_.clear();
    write_history_.clear();
    outbound_queue_.clear();
  }

  void addMockReaction(MockReaction reaction) {
    std::lock_guard<std::mutex> lock(mtx_);
    reactions_.push_back(std::move(reaction));
  }

  /**
   * @brief Injects a master message.
   * This remains synchronous because it simulates an external participant
   * and is usually called from test setup, not from inside a bus listener.
   */
  void injectMasterMessage(uint8_t source, ebus::Sequence payload) {
    payload.reduce();
    ebus::Sequence msg;
    msg.pushBack(source, false);
    msg.append(payload);
    msg.pushBack(msg.crc(), false);
    msg.extend();
    for (uint8_t b : msg) {
      bus_.writeByte(b);
    }
  }

 private:
  platform::BusSimulation& bus_;
  std::mutex mtx_;

  struct ResponseItem {
    uint8_t data[SequenceLimits::default_capacity];
    uint8_t len = 0;
    uint32_t delay_ms = 0;  // Added delay
  };

  CircularBuffer<uint8_t, SequenceLimits::default_capacity> write_history_;
  std::vector<MockReaction> reactions_;
  platform::Queue<ResponseItem> outbound_queue_;
  std::unique_ptr<platform::ServiceThread> worker_;

  // Renamed from onWrite to onRead to reflect that it observes the bus
  void onRead(uint8_t b) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_history_.push_back(b);
    for (auto& reaction : reactions_) {
      if (reaction.repeat_count == -1) continue;  // Disabled, skip

      // Check if the current history suffix matches the trigger pattern
      bool match = true;
      size_t hist_size = write_history_.size();
      size_t pat_size = reaction.trigger.size();

      if (hist_size < pat_size) {
        match = false;
      } else {
        // Compare wire-format bytes directly for Section 7 compliance
        size_t start_idx = hist_size - pat_size;
        for (size_t i = 0; i < pat_size; ++i) {
          if (write_history_[start_idx + i] != reaction.trigger[i]) {
            match = false;
            break;
          }
        }
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
