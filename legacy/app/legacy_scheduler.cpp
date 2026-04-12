/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ebus/data_types.hpp>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "utils/common.hpp"

struct TestCase {
  std::string description;
  uint8_t priority;
  // payload hex WITHOUT source byte and WITHOUT CRC; helpers will append CRC
  std::string payloadHexNoCrc;
  bool expectSuccess;
};

static std::vector<uint8_t> writeBuffer;
static std::mutex writeMutex;
static std::atomic<int> retryFailureCount{0};

bool matchTail(const std::vector<uint8_t>& buffer,
               const std::vector<uint8_t>& pattern) {
  if (buffer.size() < pattern.size()) return false;
  return std::equal(pattern.rbegin(), pattern.rend(), buffer.rbegin());
}

static uint8_t compute_crc_for_message(
    uint8_t source, const std::vector<uint8_t>& payload_no_crc) {
  uint8_t crc = source;
  for (auto b : payload_no_crc) crc = ebus::calc_crc(b, crc);
  return crc;
}

static std::string byte_to_hex(uint8_t b) {
  std::ostringstream oss;
  oss << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
      << static_cast<int>(b);
  return oss.str();
}

// Build a master message hex string (payloadHexNoCrc does not contain source or
// CRC)
static std::string msg_with_crc_hex(uint8_t source,
                                    const std::string& payloadHexNoCrc) {
  auto payload = ebus::to_vector(payloadHexNoCrc);
  uint8_t crc = compute_crc_for_message(source, payload);
  std::ostringstream oss;
  // full master: source + payload + crc
  oss << byte_to_hex(source);
  oss << payloadHexNoCrc;
  oss << byte_to_hex(crc);
  return oss.str();
}

void installSimulatorResponses(ebus::Bus& bus,
                               const std::vector<TestCase>& tests,
                               uint8_t source) {
  // Precompute expected master byte patterns for each test
  std::vector<std::vector<uint8_t>> expectedMasters;
  expectedMasters.reserve(tests.size());
  for (const auto& tc : tests) {
    expectedMasters.push_back(
        ebus::to_vector(msg_with_crc_hex(source, tc.payloadHexNoCrc)));
  }

  bus.addWriteListener([&bus, expectedMasters, tests,
                        source](const uint8_t byte) {
    std::lock_guard<std::mutex> lock(writeMutex);
    writeBuffer.push_back(byte);

    if (writeBuffer.size() > 256)
      writeBuffer.erase(writeBuffer.begin(),
                        writeBuffer.begin() + (writeBuffer.size() - 256));

    for (size_t i = 0; i < expectedMasters.size(); ++i) {
      const auto& pattern = expectedMasters[i];
      if (pattern.empty()) continue;
      if (!matchTail(writeBuffer, pattern)) continue;

      const auto& tc = tests[i];

      // Broadcast: no reply
      if (tc.description == "BC Success") {
        // nothing to do
        return;
      }

      // Master-Slave success: reply with fixed slave response
      if (tc.description == "MS Success") {
        std::thread([&bus]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          const std::vector<uint8_t> response = ebus::to_vector("00013fa4");
          for (uint8_t b : response) {
            bus.writeByte(b);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
          }
        }).detach();
        return;
      }

      // Retry test: NACK first two attempts, then valid reply
      if (tc.description == "Retry Success") {
        int count = retryFailureCount.load();
        std::cerr << "[sim] Retry match, count=" << count << std::endl;
        if (count < 2) {
          retryFailureCount.fetch_add(1);
          std::thread([&bus]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            std::cerr << "[sim] sending NACK (0xff)\n";
            bus.writeByte(0xff);
          }).detach();
        } else {
          std::thread([&bus]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            std::cerr << "[sim] sending final reply\n";
            const std::vector<uint8_t> response = ebus::to_vector("00013fa4");
            for (uint8_t b : response) {
              bus.writeByte(b);
              std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
          }).detach();
        }
        return;
      }
    }
  });
}

bool runSchedulerTest(ebus::Scheduler& scheduler, const TestCase& tc,
                      uint8_t source) {
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // Build the actual master bytes (source + payload + CRC) and pass to
  // scheduler
  scheduler.enqueue(tc.priority, ebus::to_vector(tc.payloadHexNoCrc),
                    [promise, description = tc.description](
                        bool success, const std::vector<uint8_t>& master,
                        const std::vector<uint8_t>& slave) {
                      if (!promise) return;
                      promise->set_value(success);
                      if (success) {
                        std::cout << "[PASS] " << description
                                  << ": callback success" << std::endl;
                      } else {
                        std::cout << "[FAIL] " << description
                                  << ": callback failure" << std::endl;
                      }
                    });

  // Allow longer timeout to account for SYN/backoff delays
  if (future.wait_for(std::chrono::seconds(8)) != std::future_status::ready) {
    std::cerr << "[TIMEOUT] " << tc.description
              << ": result callback did not fire" << std::endl;
    return false;
  }

  return future.get();
}

int main() {
  ebus::Request request;
  ebus::BusConfig config;
  config.device = "/dev/simulation";
  config.simulate = true;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x33;
  runtime.enable_syn = true;

  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(ebus::DEFAULT_ADDRESS, &bus, &request);
  ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

  const uint8_t source = 0x33;
  handler.setSourceAddress(source);

  // Define tests using payload (no source, no CRC)
  std::vector<TestCase> tests = {
      {"BC Success", 1, "feb5050327002d", true},
      {"MS Success", 1, "52b509030d4600", true},
      {"Retry Success", 1, "fe070400", true},
  };

  installSimulatorResponses(bus, tests, source);

  ebus::Scheduler scheduler(&handler);
  scheduler.setTelegramCallback([](const ebus::MessageType& messageType,
                                   const ebus::TelegramType& telegramType,
                                   const std::vector<uint8_t>& master,
                                   const std::vector<uint8_t>& slave) {
    std::cout << "[scheduler] telegram " << ebus::to_string(master) << " "
              << ebus::to_string(slave) << std::endl;
  });
  scheduler.setErrorCallback([](const std::string& error,
                                const std::vector<uint8_t>& master,
                                const std::vector<uint8_t>& slave) {
    std::cout << "[scheduler] error " << error << " " << ebus::to_string(master)
              << " " << ebus::to_string(slave) << std::endl;
  });

  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(std::chrono::milliseconds(50));

  bus.start();
  busHandler.start();
  scheduler.start();

  bool allPassed = true;
  for (const auto& tc : tests) {
    if (!runSchedulerTest(scheduler, tc, source)) {
      allPassed = false;
      break;
    }
  }

  scheduler.stop();
  busHandler.stop();
  bus.stop();

  if (!allPassed) {
    std::cerr << "Some scheduler tests failed." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "All scheduler tests passed!" << std::endl;
  return EXIT_SUCCESS;
}