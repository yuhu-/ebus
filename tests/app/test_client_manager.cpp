/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/types.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "app/client_manager.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/simulation/bus_simulator.hpp"
#include "platform/simulation/virtual_line.hpp"
#include "platform/socket.hpp"
#include "platform/system.hpp"
#include "test_helpers.hpp"

using namespace ebus::detail;

// Helper to mimic run_test behaviour but using Catch2
static void CHECK_TEST(const std::string& name, bool condition) {
  INFO(name);
  REQUIRE(condition);
}

TEST_CASE("ClientManager Orchestration (Regular + ReadOnly)") {
  Request request;
  request.setLockCounter(3);
  request.reset();

  ebus::BusConfig config;
  ebus::RuntimeConfig runtime{};
  runtime.address = 0x01;

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);

  // Bridge Physical Bus Events->BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  bus_handler.setReactorBusEventInfoCallback(
      [](const ebus::BusEventInfo& info) {
        std::cout << ebus::toJson(info, 256) << std::endl;
      });

  ClientManager manager(&bus, &bus_handler, &request, &bus_monitor);
  manager.setSessionTimeout(999999);
  manager.setTransmitTimeout(999999);

  int svReg[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  // Local: svReg[0], Remote: svReg[1]

  manager.addClient(std::make_unique<platform::Socket>(svReg[0]),
                    ebus::ClientType::regular);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  // Local: svRO[0], Remote: svRO[1]

  manager.addClient(std::make_unique<platform::Socket>(svRO[0]),
                    ebus::ClientType::read_only);

  bus.start();
  manager.start();

  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  bus.writeByte(ebus::Symbols::syn);

  // Send complete telegram before session starts
  for (size_t i = 0; i < telegram.size(); ++i) {
    send(svReg[1], &telegram[i], 1, 0);
  }

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(waitFor([&] { return request.busRequestPending(); }));

  // triggers next cycle and will be suppressed in Bus
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(waitFor(
      [&] { return request.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", request.getLockCounter() == 3);

  uint8_t echo;
  for (int i = 0; i < 3; ++i) {
    REQUIRE(readFromSocket(svReg[1], &echo, 1));
    CHECK_TEST("Regular received correct SYN echo", echo == ebus::Symbols::syn);
  }

  REQUIRE(readFromSocket(svReg[1], &echo, 1));
  CHECK_TEST("Regular received correct address byte echo", echo == telegram[0]);

  // Read back the rest of the telegram echoes
  for (size_t i = 1; i < telegram.size(); ++i) {
    REQUIRE(readFromSocket(svReg[1], &echo, 1));
    CHECK_TEST("Client received correct byte echo", echo == telegram[i]);
  }

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa};
  expectedRO.insert(expectedRO.end(), telegram.begin(), telegram.end());
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
}

TEST_CASE("ClientManager Enhanced Active Sending") {
  Request request;
  request.setLockCounter(3);
  request.reset();

  ebus::BusConfig config;
  ebus::RuntimeConfig runtime{};
  runtime.address = 0x01;

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent&)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  bus_handler.setReactorBusEventInfoCallback(
      [](const ebus::BusEventInfo& info) {
        std::cout << ebus::toJson(info, 256) << std::endl;
      });

  ClientManager manager(&bus, &bus_handler, &request, &bus_monitor);

  int svEnh[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  // Local: svEnh[0], Remote: svEnh[1]

  manager.addClient(std::make_unique<platform::Socket>(svEnh[0]),
                    ebus::ClientType::enhanced);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  // Local: svRO[0], Remote: svRO[1]

  manager.addClient(std::make_unique<platform::Socket>(svRO[0]),
                    ebus::ClientType::read_only);

  bus.start();
  manager.start();

  bus.writeByte(ebus::Symbols::syn);

  // Send all commands before session starts
  uint8_t cmdStart[] = {0xc8, 0xb3};
  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(svEnh[1], cmdStart, 2, 0);
  send(svEnh[1], cmdSend, 2, 0);

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(waitFor([&] { return request.busRequestPending(); }));

  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(waitFor(
      [&] { return request.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", request.getLockCounter() == 3);

  uint8_t resp[2];
  // Note: First SYN after requestBus() is filtered (one-shot), so expect 3
  // instead of 4
  for (int i = 0; i < 3; ++i) {
    readFromSocket(svEnh[1], resp, 2);
    CHECK_TEST("Enhanced received correct SYN echo",
               (resp[0] == 0xc6 && resp[1] == 0xaa));
  }

  REQUIRE(readFromSocket(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received STARTED", resp[0] == 0xc8 && resp[1] == 0xb3);

  // Read the encoded 0xfe response
  REQUIRE(readFromSocket(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received encoded 0xfe",
             resp[0] == 0xc7 && resp[1] == 0xbe);

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0x33, 0xfe};
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
}

TEST_CASE("ClientManager Watchdog Timeout") {
  Request req;
  req.setLockCounter(0);
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &req, &bus_monitor);
  BusHandler bus_handler(&req, nullptr);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent&)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  ClientManager manager(&bus, &bus_handler, &req, &bus_monitor);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(std::make_unique<platform::Socket>(sv[0]),
                    ebus::ClientType::regular);

  bus.start();
  manager.start();

  uint8_t addr = 0x33;
  send(sv[1], &addr, 1, 0);

  REQUIRE(waitFor([&] { return req.busRequestPending(); }));

  // Wait for session timeout (default 500ms) while keeping reactor alive
  platform::sleepMilli(600);

  REQUIRE(waitFor([&] { return !req.busRequestPending(); }));

  manager.stop();
  bus.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager absolute session lifetime caps retry-fed zombies",
          "[app][client]") {
  Request req;
  req.setLockCounter(0);
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &req, &bus_monitor);
  BusHandler bus_handler(&req, nullptr);

  bus.addBusEventListener(Delegate<void(const BusEvent&)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  ClientManager manager(&bus, &bus_handler, &req, &bus_monitor);
  // Idle timeout far away: only the absolute cap may kill the session.
  manager.setSessionTimeout(10000);
  manager.setTransmitTimeout(10000);
  manager.setMaxSessionAge(50);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(std::make_unique<platform::Socket>(sv[0]),
                    ebus::ClientType::regular);

  bus.start();
  manager.start();

  uint8_t addr = 0x33;
  send(sv[1], &addr, 1, 0);

  REQUIRE(waitFor([&] { return req.busRequestPending(); }));

  // Storm: SYN traffic refreshes the idle timeout continuously (the live
  // failure: bus events + pumped bytes kept a dead session alive until
  // the peer gave up). The absolute cap must still fire.
  for (int i = 0; i < 10; ++i) {
    bus.writeByte(ebus::Symbols::syn);
    platform::sleepMilli(20);
  }

  REQUIRE(waitFor([&] { return !req.busRequestPending(); }));
  bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.request.session_timeouts == 1);
  });

  manager.stop();
  bus.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager stuck intent is withdrawn and re-armed",
          "[app][client]") {
  Request req;
  req.setLockCounter(0);
  req.reset();
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &req, &bus_monitor);
  BusHandler bus_handler(&req, nullptr);
  ClientManager manager(&bus, &bus_handler, &req, &bus_monitor);
  // Idle/cap timeouts far away: only the stuck rescue may act.
  manager.setSessionTimeout(10000);
  manager.setTransmitTimeout(10000);
  manager.setMaxSessionAge(30000);
  manager.setStuckWithdrawMs(30);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(std::make_unique<platform::Socket>(sv[0]),
                    ebus::ClientType::regular);
  // Bus stays stopped: the SYN-timer path can never fire, so a branch-2
  // arm via the loop fallback sticks forever (live: freshness gate
  // deferring on a backlogged UART queue while the armed flag locks out
  // the idle fast-path). The byte stays queued (consumed only at actual
  // emission), so every rescue is followed by a clean re-arm.
  manager.start();

  uint8_t qq = 0x31;
  send(sv[1], &qq, 1, 0);
  REQUIRE(waitFor([&] { return manager.fetchStatus().session_active; }));
  // Branch-2 arms, timer never fires …
  REQUIRE(waitFor([&] { return req.busRequestPending(); }));
  // … then the rescue withdraws the unfired intent. Pending toggles as
  // withdraw and re-arm run in the same loop pass, so the monotonic
  // stuck counter — not the transient flag — is the assertion …
  REQUIRE(waitFor([&] {
    bool rescued = false;
    bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
      rescued = m.request.stuck_withdraws >= 1;
    });
    return rescued;
  }));
  bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.request.session_timeouts == 0);
  });
  // … and the session survives (rescue, not kill): the re-arm attempt
  // finds the consumed byte gone and waits quietly instead of stopping.
  REQUIRE(manager.fetchStatus().session_active == true);

  manager.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager drops bus events with no consumers", "[app][client]") {
  Request req;
  req.setLockCounter(0);
  req.reset();
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime{};
  runtime.address = 0x01;
  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &req, &bus_monitor);
  Handler handler(runtime.address, &bus, &req, &bus_monitor);
  BusHandler bus_handler(&req, &handler);

  ClientManager manager(&bus, &bus_handler, &req, &bus_monitor);
  // NOTE: not started, no clients attached: the I/O thread would only
  // spin empty loop scans per event. Events must not even queue.
  for (int i = 0; i < 10; ++i) {
    BusEvent ev{static_cast<uint8_t>(0xaa), false, false, ebus::Clock::now()};
    bus_handler.onBusEvent(ev);
  }
  ebus::ClientManagerStatus st = manager.fetchStatus();
  REQUIRE(st.bus_queue.max_size == 0);
}

TEST_CASE("ClientManager Client Removal") {
  Request req;
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &req, &bus_monitor);
  BusHandler bus_handler(&req, nullptr);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent&)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  ClientManager manager(&bus, &bus_handler, &req, &bus_monitor);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(std::make_unique<platform::Socket>(sv[1]),
                    ebus::ClientType::regular);

  close(sv[0]);

  bus.writeByte(0xaa);
  manager.start();
  platform::sleepMilli(10);

  // Reaching here indicates manager handled closed socket without hang/crash
  CHECK_TEST("Manager handled closed socket", true);

  manager.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager drops pre-grant QQ duplicates at completion",
          "[app][client]") {
  Request request;
  request.setLockCounter(0);
  request.reset();
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime{};
  runtime.address = 0x01;
  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);
  ClientManager manager(&bus, &bus_handler, &request, &bus_monitor);
  // Bus stays stopped on purpose: no SYN, no timer, no echo races. The
  // completion below is driven directly, deterministically.
  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  auto raw_client = std::make_shared<RegularClient>(
      std::make_unique<platform::Socket>(sv[0]), &request,
      ebus::RuntimeConfig{}.network.outbound_buffer_size);
  manager.addClient(raw_client);
  manager.start();

  // Session + QQ, its impatient retry twin, then continuation queued
  // before any arm (socketpair µs vs manager loop ms). Live shape: ebusd
  // re-sends QQ while the echo is slow; the twin predates the grant and
  // must not be pumped ahead of 0x08.
  const uint8_t qq = 0x31;
  REQUIRE(::write(sv[1], &qq, 1) == 1);
  REQUIRE(waitFor([&] { return manager.fetchStatus().session_active; }));
  REQUIRE(::write(sv[1], &qq, 1) == 1);
  const uint8_t cont = 0x08;
  REQUIRE(::write(sv[1], &cont, 1) == 1);

  // Branch-2 arms on the loop fallback (no pop, no transit yet) …
  REQUIRE(waitFor([&] { return request.busRequestPending(); }));
  // … then the fire's completion consumes the armed QQ and drops the
  // stale twin; the continuation survives for the pump.
  request.busRequestCompleted();
  bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.request.stuck_withdraws == 0);
    REQUIRE(m.request.stale_qq_dropped == 1);
    REQUIRE(m.request.session_timeouts == 0);
  });
  // Exact queue proof: fired QQ gone, twin dropped, continuation intact.
  uint8_t rest = 0;
  REQUIRE(raw_client->popPendingIncomingData(rest));
  REQUIRE(rest == 0x08);
  REQUIRE(!raw_client->hasPendingIncomingData());

  manager.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager external QQ fires on idle bus without SYN",
          "[app][client]") {
  Request request;
  request.setLockCounter(0);
  request.reset();
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime{};
  runtime.address = 0x01;
  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);
  ClientManager manager(&bus, &bus_handler, &request, &bus_monitor);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(std::make_unique<platform::Socket>(sv[0]),
                    ebus::ClientType::regular);

  int probe_key = 0;
  platform::VirtualLine::get().attach(&probe_key);
  platform::VirtualLine::get().clear();
  manager.start();

  // One stale SYN for the activity stamp, then silence beyond the idle
  // horizon (8ms). No further SYN is ever pumped: the QQ must still
  // reach the wire via the idle fast-path.
  bus.writeByte(ebus::Symbols::syn);
  uint8_t drain = 0;
  REQUIRE(waitCondition(
      [&]() { return platform::VirtualLine::get().read(&probe_key, drain, 0); },
      1000));
  REQUIRE(drain == ebus::Symbols::syn);
  platform::sleepMilli(15);

  // ebusd-side: QQ arrives over TCP with no SYN anywhere near.
  const uint8_t qq = 0x31;
  REQUIRE(::write(sv[1], &qq, 1) == 1);

  uint8_t seen = 0;
  const bool found = waitCondition(
      [&]() {
        uint8_t b = 0;
        while (platform::VirtualLine::get().read(&probe_key, b, 0)) {
          if (b == qq) {
            seen = b;
            return true;
          }
        }
        return false;
      },
      2000);
  REQUIRE(found == true);
  REQUIRE(seen == qq);

  // Regression: the idle fast-path must arm the intent for the emitted
  // byte. Without the arm the echo evaluates against address 0x00 and
  // scores a phantom first_lost (seen live: session killed after a
  // correctly emitted QQ).
  REQUIRE(request.busRequestAddress() == qq);
  REQUIRE(request.busRequestIsExternal() == true);

  manager.stop();
  platform::VirtualLine::get().detach(&probe_key);
  close(sv[1]);
}
