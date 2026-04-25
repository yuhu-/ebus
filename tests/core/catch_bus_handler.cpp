/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/utils.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"

TEST_CASE("BusHandler integration and behaviors", "[core][bushandler]") {
  SECTION("Integration vectors (passive/reactive/active BC happy paths)") {
    ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime;
    runtime.address = 0x01;
    runtime.bus.syn.enabled = true;

    ebus::Request request;
    ebus::BusMonitor monitor;
    ebus::Bus bus(config, runtime, &request, &monitor);
    ebus::Handler handler(runtime.address, &bus, &request, &monitor);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    std::atomic<int> error_count{0};

    handler.setTelegramCallback(
        [&](const ebus::TelegramInfo& info) { telegram_count++; });
    handler.setErrorCallback(
        [&](const ebus::ErrorInfo& info) { error_count++; });

    bus.addWriteListener(
        [&](const uint8_t b) { INFO("<- write: " << ebus::toString(b)); });
    bus.addReadListener(
        [&](const uint8_t b) { INFO("->  read: " << ebus::toString(b)); });

    bus.start();
    busHandler.start();

    struct TestCase {
      ebus::MessageType message_type;
      uint8_t address;
      std::string description;
      std::string read_string;
      std::string send_string;
      int expect_tel;
      int expect_err;
    };

    // clang-format off
    std::vector<TestCase> tests = {
        {ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000", "", 1, 0},
        {ebus::MessageType::passive, 0x33, "passive BC: Normal", "10fe07000970160443183105052592", "", 1, 0},

        {ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b", "", 1, 0},
        
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "", "feb5050427002d00", 1, 0},
    };
    // clang-format on

    for (const auto& tc : tests) {
      telegram_count.store(0);
      error_count.store(0);
      handler.reset();
      request.reset();
      bus.getQueue()->clear();
      handler.setSourceAddress(tc.address);

      if (tc.message_type == ebus::MessageType::active) {
        handler.sendActiveMessage(ebus::toVector(tc.send_string));
      } else {
        auto seq = ebus::toVector(tc.read_string);
        for (uint8_t b : seq) {
          bus.writeByte(b);
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      }

      // wait for processing (bounded)
      for (int i = 0; i < 500 && (telegram_count.load() < tc.expect_tel ||
                                  error_count.load() < tc.expect_err);
           ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      REQUIRE(telegram_count.load() == tc.expect_tel);
      REQUIRE(error_count.load() == tc.expect_err);
    }

    busHandler.stop();
    bus.stop();
  }

  SECTION("Lock counter behavior and arbitration pumping") {
    ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime = {.address = 0x33};

    ebus::Request request;
    ebus::BusMonitor monitor;
    ebus::Bus bus(config, runtime, &request, &monitor);
    ebus::Handler handler(runtime.address, &bus, &request, &monitor);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    handler.setTelegramCallback(
        [&](const ebus::TelegramInfo& info) { telegram_count++; });

    bus.start();
    busHandler.start();

    request.setMaxLockCounter(3);

    // send active BC message
    std::vector<uint8_t> msg = ebus::toVector("feb5050427002d00");
    handler.sendActiveMessage(msg);

    // Pump SYNs until arbitration starts.
    for (int i = 0; i < 4; ++i) {
      bus.writeByte(ebus::Protocol::sym_syn);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      if (handler.getState() != ebus::HandlerState::passive_receive_master)
        break;
    }

    // wait for completion
    for (int i = 0; i < 50 && telegram_count.load() == 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(telegram_count.load() == 1);

    // after release bus a SYN was generated: check lock counter behavior
    // Wait for the background BusHandler to process the trailing SYN
    for (int i = 0; i < 50 && request.getLockCounter() != 2; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(request.getLockCounter() == 2);

    // send second message and step through SYNs to force arbitration
    telegram_count.store(0);
    handler.sendActiveMessage(msg);

    bus.writeByte(ebus::Protocol::sym_syn);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(request.getLockCounter() == 1);

    bus.writeByte(ebus::Protocol::sym_syn);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(request.getLockCounter() == 0);

    bus.writeByte(ebus::Protocol::sym_syn);
    // wait for completion
    for (int i = 0; i < 50 && telegram_count.load() == 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(telegram_count.load() == 1);

    busHandler.stop();
    bus.stop();
  }

  SECTION("External client callback path") {
    ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime;
    runtime.address = 0x33;
    runtime.bus.syn.enabled = true;

    ebus::Request request;
    request.setMaxLockCounter(0);
    ebus::BusMonitor monitor;
    ebus::Bus bus(config, runtime, &request, &monitor);
    ebus::Handler handler(runtime.address, &bus, &request, &monitor);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    handler.setTelegramCallback(
        [&](const ebus::TelegramInfo& info) { telegram_count++; });

    bus.start();
    busHandler.start();

    std::atomic<bool> callbackFired{false};
    std::vector<uint8_t> clientData = ebus::toVector("feb5050427002d002c");

    request.setExternalBusRequestedCallback([&]() {
      callbackFired.store(true);
      for (uint8_t b : clientData) {
        bus.writeByte(b);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }
    });

    // request repeatedly until served; wait for callback and telegram
    for (int i = 0;
         i < 200 && !(callbackFired.load() && telegram_count.load() == 1);
         ++i) {
      if (!callbackFired.load() && !request.busRequestPending()) {
        request.requestBus(0x33, true);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(callbackFired.load() == true);
    REQUIRE(telegram_count.load() == 1);

    busHandler.stop();
    bus.stop();
  }
}