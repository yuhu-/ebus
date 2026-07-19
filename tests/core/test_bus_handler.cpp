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
#include "platform/system.hpp"

using namespace ebus::detail;

TEST_CASE("BusHandler integration and behaviors", "[core][bushandler]") {
  SECTION("Integration vectors (passive/reactive/active BC happy paths)") {
    ebus::BusConfig config;
    ebus::RuntimeConfig runtime;
    runtime.address = 0x01;
    runtime.bus.syn_gen = true;

    Request request;
    BusMonitor monitor;
    platform::Bus bus(config, runtime, &request, &monitor);
    Handler handler(runtime.address, &bus, &request, &monitor);
    BusHandler busHandler(&request, &handler);

    std::atomic<int> telegram_count{0};
    std::atomic<int> error_count{0};

    struct Stats {
      std::atomic<int>& tc;
      std::atomic<int>& ec;
      void onEvent(const ebus::ProtocolInfo& i) {
        if (i.is_error)
          ec++;
        else
          tc++;
      }
    } stats{telegram_count, error_count};

    handler.setProtocolCallback(
        Delegate<void(const ebus::ProtocolInfo& info)>::bind<Stats,
                                                             &Stats::onEvent>(
            &stats));

    // PUMP BRIDGE: Required because BusHandler is now a passive logic engine
    bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                            BusHandler, &BusHandler::onBusEvent>(&busHandler));

    bus.start();

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
      handler.setSourceAddress(tc.address);

      if (tc.message_type == ebus::MessageType::active) {
        handler.sendActiveMessage(ebus::toVector(tc.send_string));
      } else {
        auto seq = ebus::toVector(tc.read_string);
        for (uint8_t b : seq) {
          bus.writeByte(b);
          platform::sleepMicro(100);
        }
      }

      // wait for processing (bounded)
      for (int i = 0; i < 500 && (telegram_count.load() < tc.expect_tel ||
                                  error_count.load() < tc.expect_err);
           ++i) {
        platform::sleepMilli(10);
      }

      REQUIRE(telegram_count.load() == tc.expect_tel);
      REQUIRE(error_count.load() == tc.expect_err);
    }

    bus.stop();
  }

  SECTION("Lock counter behavior and arbitration pumping") {
    ebus::BusConfig config;
    ebus::RuntimeConfig runtime = {.address = 0x01};

    Request request;
    BusMonitor monitor;
    platform::Bus bus(config, runtime, &request, &monitor);
    Handler handler(runtime.address, &bus, &request, &monitor);
    BusHandler busHandler(&request, &handler);

    std::atomic<int> telegram_count{0};
    struct Stats {
      std::atomic<int>& tc;
      void onEvent(const ebus::ProtocolInfo& i) {
        if (!i.is_error) tc++;
      }
    } stats{telegram_count};
    handler.setProtocolCallback(
        Delegate<void(const ebus::ProtocolInfo& info)>::bind<Stats,
                                                             &Stats::onEvent>(
            &stats));

    // PUMP BRIDGE: Required because BusHandler is now a passive logic engine
    bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                            BusHandler, &BusHandler::onBusEvent>(&busHandler));

    bus.start();

    request.setLockCounter(3);

    // send active BC message
    std::vector<uint8_t> msg = ebus::toVector("feb5050427002d00");
    handler.sendActiveMessage(msg);

    // Pump SYNs until arbitration starts.
    // Needs 4:
    // we start with 3 after
    // first SYN:  2
    // second SYN: 1
    // third SYN:  0 we request bus
    // forth SYN:  0 we fire address byte 200us delayed on the bus
    for (int i = 0; i < 4; ++i) {
      bus.writeByte(ebus::Symbols::syn);
      platform::sleepMilli(50);
      if (handler.getState() != ebus::HandlerState::passive_receive_master)
        break;
    }

    // wait for completion
    for (int i = 0; i < 50 && telegram_count.load() == 0; ++i)
      platform::sleepMilli(10);

    REQUIRE(telegram_count.load() == 1);
    REQUIRE(request.getLockCounter() == 2);

    // send second message and step through SYNs to force arbitration
    telegram_count.store(0);
    handler.sendActiveMessage(msg);

    bus.writeByte(ebus::Symbols::syn);
    platform::sleepMilli(20);
    REQUIRE(request.getLockCounter() == 1);

    bus.writeByte(ebus::Symbols::syn);
    platform::sleepMilli(20);
    REQUIRE(request.getLockCounter() == 0);  // request bus

    bus.writeByte(ebus::Symbols::syn);  // fire bus 200us  delayed

    // wait for completion
    for (int i = 0; i < 50 && telegram_count.load() == 0; ++i)
      platform::sleepMilli(10);

    REQUIRE(telegram_count.load() == 1);
    platform::sleepMilli(20);
    REQUIRE(request.getLockCounter() == 2);

    bus.stop();
  }

  SECTION("External client callback path") {
    ebus::BusConfig config;
    ebus::RuntimeConfig runtime;
    runtime.address = 0x01;
    runtime.bus.syn_gen = true;

    Request request;
    request.setLockCounter(0);
    BusMonitor monitor;
    platform::Bus bus(config, runtime, &request, &monitor);
    Handler handler(runtime.address, &bus, &request, &monitor);
    BusHandler busHandler(&request, &handler);

    std::atomic<int> telegram_count{0};
    struct Stats {
      std::atomic<int>& tc;
      void onEvent(const ebus::ProtocolInfo& i) {
        if (!i.is_error) tc++;
      }
    } stats{telegram_count};
    handler.setProtocolCallback(
        Delegate<void(const ebus::ProtocolInfo& info)>::bind<Stats,
                                                             &Stats::onEvent>(
            &stats));

    // PUMP BRIDGE: Required because BusHandler is now a passive logic engine
    bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                            BusHandler, &BusHandler::onBusEvent>(&busHandler));

    bus.start();

    std::atomic<bool> callbackFired{false};
    std::vector<uint8_t> clientData = ebus::toVector("feb5050427002d002c");

    struct ExternalReq {
      std::atomic<bool>& fired;
      std::vector<uint8_t>& data;
      platform::Bus& b;
      void onReq() {
        fired.store(true);
        for (uint8_t val : data) {
          b.writeByte(val);
          platform::sleepMicro(500);
        }
      }
    } extReq{callbackFired, clientData, bus};

    request.setExternalBusRequestedCallback(
        Delegate<void()>::bind<ExternalReq, &ExternalReq::onReq>(&extReq));

    // request repeatedly until served; wait for callback and telegram
    for (int i = 0;
         i < 200 && !(callbackFired.load() && telegram_count.load() == 1);
         ++i) {
      if (!callbackFired.load() && !request.busRequestPending()) {
        request.requestBus(0x33, true);
      }
      platform::sleepMilli(10);
    }

    REQUIRE(callbackFired.load() == true);
    REQUIRE(telegram_count.load() == 1);

    bus.stop();
  }
}