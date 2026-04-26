/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>

#include <app/client.hpp>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/bus.hpp"
#include "platform/service_thread.hpp"
#include "platform/system.hpp"

namespace ebus::detail {

/**
 * Robust read helper to handle partial TCP/Socket reads.
 */
inline bool readExact(int fd, uint8_t* buffer, size_t length) {
  size_t total = 0;
  while (total < length) {
    ssize_t n = read(fd, buffer + total, length - total);
    if (n <= 0) return false;
    total += n;
  }
  return true;
}

/**
 * Helper to wait for a condition to become true without hardcoded sleeps.
 */
template <typename Predicate>
inline bool waitCondition(Predicate&& pred, int timeout_ms = 1000) {
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <
         std::chrono::milliseconds(timeout_ms)) {
    if (pred()) return true;
    sleepMilli(5);
  }
  return pred();
}

/**
 * Helper to frame a master telegram with CRC for testing.
 */
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

/**
 * Helper to frame a slave response with CRC for testing.
 * Includes the leading ACK (0x00).
 */
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
 * In-memory client for testing ClientManager without real sockets.
 */
class MockClient : public detail::AbstractClient {
 public:
  explicit MockClient(Request* req, bool write_capable = true,
                      size_t max_buffer = 1024)
      : AbstractClient(999, req, write_capable, max_buffer) {}

  ~MockClient() override { fd_ = -1; }

  bool wantsToSend() override { return !inbound_.empty(); }
  bool recvFromClient(uint8_t& out) override {
    if (inbound_.empty()) return false;
    out = inbound_.front();
    inbound_.pop();
    return true;
  }
  void sendToClient(ByteView data) override {
    if (outbound_.size() + data.size() > max_buffer_size_) {
      this->stop();  // Simulate socket closing on overflow
      return;
    }
    outbound_.insert(outbound_.end(), data.begin(), data.end());
  }

  BridgeAction onBusByte(const BusEventContext& ctx) override {
    if (!this->isConnected()) return BridgeAction::stop_session;
    sendToClient(ByteView(&ctx.byte, 1));
    return BridgeAction::keep_active;
  }

  void pushInput(uint8_t b) { inbound_.push(b); }
  const std::vector<uint8_t>& getOutput() const { return outbound_; }

 private:
  std::queue<uint8_t> inbound_;
  std::vector<uint8_t> outbound_;
};

/**
 * Automated responder for Bus simulation.
 */
class BusSimulator {
 public:
  explicit BusSimulator(Bus& bus) : bus_(bus) {
    bus_.addWriteListener([this](uint8_t b) { this->onWrite(b); });
  }

  ~BusSimulator() {
    for (auto& w : response_workers_) {
      if (w) w->join();
    }
  }

  struct AutoResponse {
    std::vector<uint8_t> trigger_pattern;
    std::vector<uint8_t> response_data;
    uint32_t delay_ms = 5;
    int repeat_count = 1;  // 0 for infinite
  };

  void addResponse(AutoResponse resp) {
    std::lock_guard<std::mutex> lock(mtx_);
    responses_.push_back(std::move(resp));
  }

  /**
   * Shortcut for creating a scripted Master-Slave response.
   * @param source The source master address.
   * @param masterPayloadHex The hex string of master data (ZZ PB SB NN DB...).
   * @param slavePayloadHex The hex string of slave data (NN DB...).
   */
  void addMasterSlaveResponse(uint8_t source,  // NOLINT
                              const std::string& masterPayloadHex,
                              const std::string& slavePayloadHex,
                              uint32_t delay_ms = 5) {
    addResponse({ebus::toVector(frameMasterHex(source, masterPayloadHex)),
                 ebus::toVector(frameSlaveHex(slavePayloadHex)), delay_ms});
  }

  void clear() {
    std::vector<std::unique_ptr<ServiceThread>> workers_to_join;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      responses_.clear();
      write_history_.clear();
      workers_to_join = std::move(response_workers_);
    }
    for (auto& w : workers_to_join) {
      w->join();
    }
  }

 private:
  Bus& bus_;
  std::mutex mtx_;
  std::vector<uint8_t> write_history_;
  std::vector<AutoResponse> responses_;
  std::vector<std::unique_ptr<ServiceThread>> response_workers_;

  void onWrite(uint8_t b) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_history_.push_back(b);
    if (write_history_.size() > 64)
      write_history_.erase(write_history_.begin());

    for (auto& resp : responses_) {
      bool infinite = (resp.repeat_count == 0);
      if (infinite || resp.repeat_count > 0) {
        if (matches(write_history_, resp.trigger_pattern,
                    write_history_.size() - resp.trigger_pattern.size())) {
          if (!infinite) resp.repeat_count--;

          uint32_t delay = resp.delay_ms;
          std::vector<uint8_t> data = resp.response_data;  // NOLINT

          auto worker = std::make_unique<ServiceThread>(
              "busSimResp", [this, delay, data]() {
                sleepMilli(delay);
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
