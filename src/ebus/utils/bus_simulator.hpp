/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "core/telegram.hpp"
#include "platform/bus.hpp"
#include "platform/service_thread.hpp"
#include "platform/system.hpp"

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
  explicit BusSimulator(platform::Bus& bus) : bus_(bus) {
    bus_.addWriteListener([this](uint8_t b) { this->onWrite(b); });
  }

  ~BusSimulator() {
    for (auto& w : response_workers_) {
      if (w) w->join();
    }
  }

  void addResponse(ebus::VirtualBus::AutoResponse resp) {
    std::lock_guard<std::mutex> lock(mtx_);
    responses_.push_back(std::move(resp));
  }

  void addMasterSlaveResponse(uint8_t source,
                              const std::string& masterPayloadHex,
                              const std::string& slavePayloadHex,
                              uint32_t delay_ms = 5) {
    addResponse({ebus::toVector(frameMasterHex(source, masterPayloadHex)),
                 ebus::toVector(frameSlaveHex(slavePayloadHex)), delay_ms});
  }

  void clear() {
    std::vector<std::unique_ptr<platform::ServiceThread>> workers_to_join;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      responses_.clear();
      write_history_.clear();
      workers_to_join = std::move(response_workers_);
      response_workers_.clear();  // Clear the original vector
    }
    for (auto& w : workers_to_join) {
      if (w) w->join();
    }
  }

  void injectMasterMessage(uint8_t source, ebus::Sequence payload) {
    bus_.writeByte(ebus::Symbols::syn);
    platform::sleepMicro(100);

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
  platform::Bus& bus_;
  std::mutex mtx_;
  std::vector<uint8_t> write_history_;
  std::vector<ebus::VirtualBus::AutoResponse> responses_;
  std::vector<std::unique_ptr<platform::ServiceThread>> response_workers_;

  void onWrite(uint8_t b) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_history_.push_back(b);
    if (write_history_.size() > SequenceLimits::default_capacity)
      write_history_.erase(write_history_.begin());

    for (auto& resp : responses_) {
      bool infinite = (resp.repeat_count == 0);
      if (infinite || resp.repeat_count > 0) {
        if (ebus::matches(
                write_history_, resp.trigger_pattern,
                write_history_.size() - resp.trigger_pattern.size())) {
          if (!infinite) resp.repeat_count--;

          uint32_t delay = resp.delay_ms;
          std::vector<uint8_t> data = resp.response_data;

          auto worker = std::make_unique<platform::ServiceThread>(
              "ebus_bus_simulator_resp", [this, delay, data]() {
                platform::sleepMilli(delay);
                for (uint8_t byte : data) bus_.writeByte(byte);
              });
          worker->start();
          response_workers_.push_back(std::move(worker));
        }
      }
    }
  }
};

}  // namespace ebus::detail
