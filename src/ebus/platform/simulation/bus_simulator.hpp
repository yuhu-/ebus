/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if EBUS_SIMULATION
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/virtual_bus.hpp>
#include <memory>
#include <vector>

#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "utils/circular_buffer.hpp"

// Forward declarations for types used in private members
namespace ebus::detail::platform {
class BusSimulation;
class ServiceThread;
}  // namespace ebus::detail::platform

namespace ebus::detail {

/**
 * Automated responder for Bus simulation. This is an internal utility.
 */
class BusSimulator {
 public:
  // Lifecycle
  explicit BusSimulator(platform::BusSimulation& bus);
  ~BusSimulator();
  void stop();

  // Working Methods
  void clear();

  uint32_t addMockReaction(VirtualBus::MockReaction reaction);
  void removeMockReaction(uint32_t id);
  void removeMockReaction(const ebus::Sequence& trigger);

  void injectRawByte(uint8_t byte);
  void injectCollision(uint8_t byte1, uint8_t byte2);

  /**
   * @brief Injects a master message.
   * This remains synchronous because it simulates an external participant
   * and is usually called from test setup, not from inside a bus listener.
   */
  void injectMasterMessage(uint8_t source, ByteView payload);

  /**
   * @brief Injects a complete master-slave message exchange onto the bus.
   */
  void injectMasterSlaveMessage(uint8_t source, ByteView master_payload,
                                ByteView slave_payload);

 private:
  platform::BusSimulation& bus_;
  platform::Mutex mutex_;
  uint32_t next_reaction_id_ = 0;

  struct ResponseItem {
    uint8_t data[SequenceLimits::default_capacity];
    uint8_t len = 0;
    uint32_t delay_ms = 0;
    uint32_t delay_us = 0;
    bool wait_for_syn = false;
  };

  CircularBuffer<uint8_t, SequenceLimits::default_capacity> write_history_;
  std::vector<VirtualBus::MockReaction> reactions_;
  platform::Queue<ResponseItem> outbound_queue_;
  std::unique_ptr<detail::platform::ServiceThread> worker_;

  platform::ConditionVariable syn_cv_;
  bool syn_received_ = false;

  void onRead(const uint8_t& b);
  void processResponses();
};

}  // namespace ebus::detail

#endif  // EBUS_SIMULATION
