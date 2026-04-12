/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/definitions.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/system.hpp"

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
 * ProtocolLogger: Captures eBUS traffic and state changes.
 * In Catch2: Messages are captured via INFO() and only shown on failure.
 * In Legacy/Dev: Messages can be forced to std::cout for real-time insights.
 */
class ProtocolLogger {
 public:
  enum class Mode {
    CatchOnly,  // Only uses Catch2 INFO (silent unless test fails)
    DevTrace    // Synchronous stdout (like legacy tests)
  };

  ProtocolLogger(ebus::Bus* bus, ebus::Handler* handler,
                 Mode mode = Mode::CatchOnly)
      : mode_(mode) {
    if (bus) {
      bus->addWriteListener([this](const uint8_t& b) { log("<- write", b); });
      bus->addReadListener([this](const uint8_t& b) { log("->  read", b); });
    }
    if (handler) {
      handler->setTelegramCallback(
          [this](const ebus::MessageType& mt, const ebus::TelegramType& tt,
                 const std::vector<uint8_t>& m,
                 const std::vector<uint8_t>& s) { logTelegram(mt, tt, m, s); });
    }
  }

 private:
  void log(const std::string& prefix, uint8_t byte) {
    std::string msg = prefix + ": " + ebus::toString(byte);
    if (mode_ == Mode::DevTrace) {
      std::cout << "[TRACE] " << msg << std::endl;
    }
    // This remains visible in Catch2 metadata if the test fails
    UNSCOPED_INFO(msg);
  }

  void logTelegram(const ebus::MessageType& mt, const ebus::TelegramType& tt,
                   const std::vector<uint8_t>& m,
                   const std::vector<uint8_t>& s) {
    std::stringstream ss;
    ss << "Telegram ["
       << (mt == ebus::MessageType::active ? "Active" : "Passive")
       << "] Type: " << static_cast<int>(tt) << " Master: " << ebus::toString(m)
       << " Slave: " << ebus::toString(s);

    if (mode_ == Mode::DevTrace) {
      std::cout << "\033[1;32m[LOG] " << ss.str() << "\033[0m" << std::endl;
    }
    UNSCOPED_INFO(ss.str());
  }

  Mode mode_;
};

// Usage example in a test case:
/*
TEST_CASE("ClientManager Orchestration", "[app]") {
    // ... setup ...
    ebus::Bus bus(config, runtime, &req);
    ebus::Handler handler(runtime.address, &bus, &req);

    // Instantiate logger - it wires itself up via listeners
    ProtocolLogger logger(&bus, &handler);

    // If this fails, the logger output appears automatically in the report
    REQUIRE(handler.getState() == ebus::HandlerState::activeSendMaster);
}


// Change mode to DevTrace to see real-time stdout during execution
ProtocolLogger logger(&bus, &handler, ProtocolLogger::Mode::DevTrace);
*/