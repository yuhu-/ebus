/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "Core/BusHandler.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "Utils/Common.hpp"

TEST_CASE("BusHandler integration and behaviors", "[core][bushandler]") {
  SECTION("Integration vectors (passive/reactive/active BC happy paths)") {
    ebus::busConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime{
        .address = 0x01, .window = 50, .offset = 5, .enable_syn = true};
    ebus::Request request;
    ebus::Bus bus(config, runtime, &request);
    ebus::Handler handler(runtime.address, &bus, &request);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    std::atomic<int> error_count{0};

    handler.setTelegramCallback(
        [&](const ebus::MessageType&, const ebus::TelegramType&,
            const std::vector<uint8_t>&,
            const std::vector<uint8_t>&) { telegram_count++; });
    handler.setErrorCallback(
        [&](const std::string&, const std::vector<uint8_t>&,
            const std::vector<uint8_t>&) { error_count++; });

    bus.addWriteListener(
        [&](const uint8_t b) { INFO("<- write: " << ebus::to_string(b)); });
    bus.addReadListener(
        [&](const uint8_t b) { INFO("->  read: " << ebus::to_string(b)); });

    bus.start();
    busHandler.start();

    struct TestCase {
      ebus::MessageType messageType;
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

      if (tc.messageType == ebus::MessageType::active) {
        handler.sendActiveMessage(ebus::to_vector(tc.send_string));
      } else {
        auto seq = ebus::to_vector(tc.read_string);
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
    ebus::busConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime{
        .address = 0xff, .window = 50, .offset = 5, .enable_syn = true};

    ebus::Request request;
    ebus::Bus bus(config, runtime, &request);
    ebus::Handler handler(runtime.address, &bus, &request);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    handler.setTelegramCallback(
        [&](const ebus::MessageType&, const ebus::TelegramType&,
            const std::vector<uint8_t>&,
            const std::vector<uint8_t>&) { telegram_count++; });

    bus.start();
    busHandler.start();

    request.setMaxLockCounter(3);

    // send active BC message
    std::vector<uint8_t> msg = ebus::to_vector("feb5050427002d00");
    handler.sendActiveMessage(msg);

    // pump SYNs until first completion or timeout
    for (int i = 0; i < 20 && telegram_count.load() == 0; ++i) {
      bus.writeByte(ebus::sym_syn);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    REQUIRE(telegram_count.load() == 1);

    // after release bus a SYN was generated: check lock counter behavior
    // Expect it to have been reset then decremented by that SYN -> 2
    REQUIRE(request.getLockCounter() == 2);

    // send second message and step through SYNs to force arbitration
    telegram_count.store(0);
    handler.sendActiveMessage(msg);

    bus.writeByte(ebus::sym_syn);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(request.getLockCounter() == 1);

    bus.writeByte(ebus::sym_syn);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(request.getLockCounter() == 0);

    bus.writeByte(ebus::sym_syn);
    // wait for completion
    for (int i = 0; i < 50 && telegram_count.load() == 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(telegram_count.load() == 1);

    busHandler.stop();
    bus.stop();
  }

  SECTION("External client callback path") {
    ebus::busConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime{
        .address = 0x33, .window = 50, .offset = 5, .enable_syn = true};

    ebus::Request request;
    ebus::Bus bus(config, runtime, &request);
    ebus::Handler handler(runtime.address, &bus, &request);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    std::atomic<int> telegram_count{0};
    handler.setTelegramCallback(
        [&](const ebus::MessageType&, const ebus::TelegramType&,
            const std::vector<uint8_t>&,
            const std::vector<uint8_t>&) { telegram_count++; });

    bus.start();
    busHandler.start();

    std::atomic<bool> callbackFired{false};
    std::vector<uint8_t> clientData = ebus::to_vector("feb5050427002d002c");

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