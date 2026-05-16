/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(EBUS_SIMULATION)
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/sequence.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <ebus/virtual_bus.hpp>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "core/telegram.hpp"
#include "platform/service_thread.hpp"
#include "platform/simulation/bus_simulation.hpp"
#include "utils/circular_buffer.hpp"

namespace ebus::detail {

// Helper to frame a master telegram with CRC for testing.
inline std::string frameMasterHex(uint8_t source,
                                  const std::string& payloadHex) {
  auto payload = ebus::toVector(payloadHex);
  uint8_t crc = source;
  for (auto b : payload) crc = ebus::calcCRC(b, crc);

  std::ostringstream oss;
  oss << std::hex << std::setw(2) << std::setfill('0')
      << static_cast<int>(source);
  oss << payloadHex;
  oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(crc);
  return oss.str();
}

// Helper to frame a slave response with CRC for testing.
// Includes the leading ACK (0x00).
inline std::string frameSlaveHex(const std::string& payloadHex) {
  auto payload = ebus::toVector(payloadHex);
  uint8_t crc = 0;
  // Spec 4.2: CRC starts with NN (the first byte of the slave payload)
  for (auto b : payload) crc = ebus::calcCRC(b, crc);

  std::ostringstream oss;
  oss << "00";  // ACK
  oss << payloadHex;
  oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(crc);
  return oss.str();
}

/**
 * Automated responder for Bus simulation. This is an internal utility.
 */
class BusSimulator {
 public:
  explicit BusSimulator(platform::BusSimulation& bus)
      : bus_(bus), outbound_queue_(16) {
    bus_.addWriteListener([this](uint8_t b) { this->onWrite(b); });
    worker_ = std::make_unique<platform::ServiceThread>(
        "ebus_sim_worker", [this] { processResponses(); },
        OrchestrationLimits::default_stack_size,
        OrchestrationLimits::default_priority);
    worker_->start();
  }

  ~BusSimulator() {
    outbound_queue_.shutdown();
    if (worker_) worker_->join();
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

  void addResponse(ebus::VirtualBus::AutoResponse resp) {
    std::lock_guard<std::mutex> lock(mtx_);
    responses_.push_back(std::move(resp));
  }

  void addResponse(uint8_t source, const std::string& masterPayloadHex,
                   const std::string& slavePayloadHex) {
    addResponse({ebus::toVector(frameMasterHex(source, masterPayloadHex)),
                 ebus::toVector(frameSlaveHex(slavePayloadHex)), 0});
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    responses_.clear();
    write_history_.clear();
    outbound_queue_.clear();
  }

 private:
  platform::BusSimulation& bus_;
  std::mutex mtx_;

  CircularBuffer<uint8_t, SequenceLimits::default_capacity> write_history_;
  std::vector<ebus::VirtualBus::AutoResponse> responses_;
  platform::Queue<std::vector<uint8_t>> outbound_queue_;
  std::unique_ptr<platform::ServiceThread> worker_;

  void onWrite(uint8_t b) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_history_.push_back(b);
    for (auto& resp : responses_) {
      // Check if the current history suffix matches the trigger pattern
      bool match = true;
      if (write_history_.size() < resp.trigger_pattern.size())
        match = false;
      else {
        size_t hist_size = write_history_.size();
        size_t pat_size = resp.trigger_pattern.size();
        for (size_t i = 0; i < pat_size; ++i) {
          if (write_history_[hist_size - pat_size + i] !=
              resp.trigger_pattern[i]) {
            match = false;
            break;
          }
        }
      }

      if (match) {
        bool infinite = (resp.repeat_count == 0);
        if (infinite || resp.repeat_count > 0) {
          if (!infinite) resp.repeat_count--;
          // Non-blocking push. If the simulator is overwhelmed, we skip.
          outbound_queue_.tryPush(resp.response_data);
        }
      }
    }
  }

  void processResponses() {
    std::vector<uint8_t> data;
    // Blocking pop handles the sleep/wait automatically
    while (outbound_queue_.pop(data)) {
      for (uint8_t byte : data) {
        bus_.writeByte(byte);
      }
    }
  }
};

}  // namespace ebus::detail

#endif  // EBUS_SIMULATION
