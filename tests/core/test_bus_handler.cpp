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
#include "platform/simulation/virtual_line.hpp"
#include "platform/system.hpp"

using namespace ebus::detail;

TEST_CASE("BusHandler integration and behaviors", "[core][bushandler]") {
  SECTION("Integration vectors (passive/reactive/active BC happy paths)") {
    ebus::BusConfig config;
    ebus::RuntimeConfig runtime;
    runtime.address = 0x01;
    runtime.bus.syn_gen = true;

    Request request;
    BusMonitor bus_monitor;
    platform::Bus bus(config, runtime, &request, &bus_monitor);
    Handler handler(runtime.address, &bus, &request, &bus_monitor);
    BusHandler bus_handler(&request, &handler);

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
                            BusHandler, &BusHandler::onBusEvent>(&bus_handler));

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
    BusMonitor bus_monitor;
    platform::Bus bus(config, runtime, &request, &bus_monitor);
    Handler handler(runtime.address, &bus, &request, &bus_monitor);
    BusHandler bus_handler(&request, &handler);

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
                            BusHandler, &BusHandler::onBusEvent>(&bus_handler));

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
    BusMonitor bus_monitor;
    platform::Bus bus(config, runtime, &request, &bus_monitor);
    Handler handler(runtime.address, &bus, &request, &bus_monitor);
    BusHandler bus_handler(&request, &handler);

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
                            BusHandler, &BusHandler::onBusEvent>(&bus_handler));

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

#if EBUS_SIMULATION
// Direct-drive (no threads/timing): feeds BusEvents straight into
// BusHandler to prove the post-won preload semantics deterministically.
struct PreloadDriver {
  Request request;
  BusMonitor bus_monitor;
  platform::Bus bus;
  Handler handler;
  BusHandler bus_handler;
  int probe_key = 0;

  struct Stats {
    int telegrams = 0;
    int errors = 0;
    ebus::ProtocolError last_error = ebus::ProtocolError::none;
    void onEvent(const ebus::ProtocolInfo& i) {
      if (i.is_error) {
        errors++;
        last_error = i.protocol_error;
      } else {
        telegrams++;
      }
    }
  } stats;

  PreloadDriver()
      : bus(ebus::BusConfig{}, ebus::RuntimeConfig{}, &request, &bus_monitor),
        handler(0x33, &bus, &request, &bus_monitor),
        bus_handler(&request, &handler) {
    request.setLockCounter(0);
    request.reset();
    handler.reset();
    handler.setSourceAddress(0x33);
    handler.setProtocolCallback(
        Delegate<void(const ebus::ProtocolInfo& info)>::bind<Stats,
                                                             &Stats::onEvent>(
            &stats));
    platform::VirtualLine::get().attach(&probe_key);
    platform::VirtualLine::get().clear();
  }

  ~PreloadDriver() { platform::VirtualLine::get().detach(&probe_key); }

  void feed(uint8_t byte, bool bus_request = false, bool start_bit = false) {
    BusEvent ev{byte, bus_request, start_bit, ebus::Clock::now()};
    bus_handler.onBusEvent(ev);
  }

  // Drives arbitration to won() for source 0x33. Returns after the win.
  // With expect_active=false (stale win) the release lands back passive.
  void driveToWon(bool expect_active = true) {
    REQUIRE(handler.sendActiveMessage(ebus::toVector("feb5050427002d00")) ==
            true);
    feed(ebus::Symbols::syn);        // request armed
    feed(ebus::Symbols::syn, true);  // -> first
    feed(0x33);                      // first_won -> won()
    REQUIRE(handler.getState() ==
            (expect_active ? ebus::HandlerState::active_send_master
                           : ebus::HandlerState::passive_receive_master));
  }

  std::vector<uint8_t> drainWire() {
    std::vector<uint8_t> out;
    uint8_t b = 0;
    while (platform::VirtualLine::get().read(&probe_key, b, 0)) {
      // Skip our own arbitration echo reads?? No: probe sees every write,
      // including the QQ echo path... every bus_->write lands here.
      out.push_back(b);
    }
    return out;
  }
};

TEST_CASE("Handler preload: full master remainder on wire after won",
          "[core][bushandler][preload]") {
  PreloadDriver d;
  d.driveToWon();

  // Zero further run() calls: the whole remainder must already sit on the
  // wire (single wakeup). Old code would have placed exactly one byte.
  std::vector<uint8_t> wire = d.drainWire();
  REQUIRE(wire.size() > 1);

  // The win itself is recorded for the qq_win_age distribution.
  d.bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.request.qq_win_age.count >= 1);
  });

  // Feed the bulk back as echoes: must complete without further writes.
  for (uint8_t b : wire) d.feed(b);
  REQUIRE(d.stats.telegrams == 1);
  REQUIRE(d.stats.errors == 0);
}

TEST_CASE("Handler preload: idx1 mismatch aborts and re-arms cleanly",
          "[core][bushandler][preload]") {
  PreloadDriver d;
  d.driveToWon();
  std::vector<uint8_t> wire = d.drainWire();
  REQUIRE(wire.size() > 1);

  // Corrupt the second byte echo (clean SYN, the field signature).
  d.feed(wire[0]);
  d.feed(ebus::Symbols::syn);
  REQUIRE(d.stats.errors == 1);
  REQUIRE(d.stats.last_error == ebus::ProtocolError::error_active_master_echo);
  REQUIRE(d.stats.telegrams == 0);

  // Preload flag must not leak: a fresh transfer wins and completes.
  d.request.reset();
  d.handler.reset();
  d.driveToWon();
  wire = d.drainWire();
  for (uint8_t b : wire) d.feed(b);
  REQUIRE(d.stats.telegrams == 1);
}

TEST_CASE("Handler preload: stale won releases silently without bulk",
          "[core][bushandler][preload]") {
  PreloadDriver d;
  // Threshold 0: every win is stale (BusSimulation age is 0, never stale
  // by itself, so this forces the branch deterministically).
  d.handler.setQqStaleThresholdUs(0);

  struct Lost {
    int count = 0;
    void onLost() { count++; }
  } lost;
  d.handler.setBusRequestLostCallback(
      Delegate<void()>::bind<Lost, &Lost::onLost>(&lost));

  d.driveToWon(false);
  // ...no bulk (or anything) staged: the wire stays empty...
  REQUIRE(d.drainWire().empty());
  // ...scheduler learns via lost, not via error:
  REQUIRE(lost.count == 1);
  REQUIRE(d.stats.errors == 0);
  REQUIRE(d.stats.telegrams == 0);
}

TEST_CASE("Handler errors: every error event pairs with a counter",
          "[core][bushandler][pairing]") {
  PreloadDriver d;
  d.driveToWon();
  // Framing noise mid-transfer: start bit clears the buffers but keeps
  // the state, so the next echo hits active_send_master with an empty
  // master (the overnight 840 ghost with error_total 0).
  std::vector<uint8_t> wire = d.drainWire();
  d.feed(wire[0]);
  REQUIRE(d.stats.errors == 0);
  // Start-bit framing noise clears the buffers but keeps the state, so
  // this same byte hits active_send_master with an empty master (the
  // overnight 840 ghost with error_total 0).
  d.feed(wire[1], false, true);  // start-bit error event
  REQUIRE(d.stats.errors == 1);
  REQUIRE(d.stats.last_error == ebus::ProtocolError::illegal_fsm_transition);
  // Pairing rule: top_errors and error_total move together — fetch both
  // from the same snapshot and compare.
  d.bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    const uint32_t e_total = m.handler.error_passive +
                             m.handler.error_reactive + m.handler.error_active;
    uint32_t top_total = 0;
    for (const auto& e : m.handler.top_errors) top_total += e.count;
    REQUIRE(e_total == top_total);
    REQUIRE(e_total == 1);
  });
}
#endif  // EBUS_SIMULATION