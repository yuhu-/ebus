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
#include "platform/socket.hpp"
#include "platform/system.hpp"
#include "test_utils.hpp"

using namespace ebus::detail;

// Helper to mimic run_test behaviour but using Catch2
static void CHECK_TEST(const std::string& name, bool condition) {
  INFO(name);
  REQUIRE(condition);
}

TEST_CASE("ClientManager Orchestration (Regular + ReadOnly)") {
  Request req;
  req.setLockCounter(3);
  req.reset();

  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0x01};

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  Handler handler(runtime.address, &bus, &req, &monitor);
  BusHandler busHandler(&req, &handler);
  platform::Queue<ebus::OrchestrationEvent> reactor_queue(32);
  ClientManager manager(&bus, &busHandler, &req, &monitor, &reactor_queue);

  int svReg[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  manager.addClient(svReg[0], ebus::ClientType::regular);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  manager.addClient(svRO[0], ebus::ClientType::read_only);

  bus.start();
  manager.start();

  TestReactor reactor(bus, busHandler, &manager, nullptr, &reactor_queue);

  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  bus.writeByte(ebus::Symbols::syn);

  send(svReg[1], &telegram[0], 1, 0);

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor([&] { return req.busRequestPending(); }));

  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor(
      [&] { return req.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t echo;
  for (int i = 0; i < 4; ++i) {
    REQUIRE(reactor.readFromSocket(svReg[1], &echo, 1));
    CHECK_TEST("Regular received correct SYN echo", echo == ebus::Symbols::syn);
  }

  REQUIRE(reactor.readFromSocket(svReg[1], &echo, 1));
  CHECK_TEST("Regular received correct address byte echo", echo == telegram[0]);

  for (size_t i = 1; i < telegram.size(); ++i) {
    send(svReg[1], &telegram[i], 1, 0);

    REQUIRE(reactor.readFromSocket(svReg[1], &echo, 1));
    CHECK_TEST("Client received correct byte echo", echo == telegram[i]);
  }

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa};
  expectedRO.insert(expectedRO.end(), telegram.begin(), telegram.end());
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
  close(svReg[1]);
  close(svRO[1]);
}

TEST_CASE("ClientManager Enhanced Active Sending") {
  Request req;
  req.setLockCounter(3);
  req.reset();

  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0x01};

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  Handler handler(runtime.address, &bus, &req, &monitor);
  BusHandler busHandler(&req, &handler);
  platform::Queue<ebus::OrchestrationEvent> reactor_queue(32);
  ClientManager manager(&bus, &busHandler, &req, &monitor, &reactor_queue);

  int svEnh[2], svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);

  manager.addClient(svEnh[0], ebus::ClientType::enhanced);
  manager.addClient(svRO[0], ebus::ClientType::read_only);

  bus.start();
  manager.start();

  TestReactor reactor(bus, busHandler, &manager, nullptr, &reactor_queue);

  bus.writeByte(ebus::Symbols::syn);

  uint8_t cmdStart[] = {0xc8, 0xb3};
  send(svEnh[1], cmdStart, 2, 0);

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor([&] { return req.busRequestPending(); }));

  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor(
      [&] { return req.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t resp[2];
  for (int i = 0; i < 4; ++i) {
    reactor.readFromSocket(svEnh[1], resp, 2);
    CHECK_TEST("Enhanced received correct SYN echo",
               (resp[0] == 0xc6 && resp[1] == 0xaa));
  }

  REQUIRE(reactor.readFromSocket(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received STARTED", resp[0] == 0xc8 && resp[1] == 0xb3);

  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(svEnh[1], cmdSend, 2, 0);
  REQUIRE(reactor.waitFor([&] { return true; }, std::chrono::milliseconds(10)));

  REQUIRE(reactor.readFromSocket(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received encoded 0xfe",
             resp[0] == 0xc7 && resp[1] == 0xbe);

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa, 0x33, 0xfe};
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
  close(svEnh[1]);
  close(svRO[1]);
}

TEST_CASE("ClientManager Watchdog Timeout") {
  Request req;
  req.setLockCounter(0);
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  BusHandler busHandler(&req, nullptr);
  platform::Queue<ebus::OrchestrationEvent> reactor_queue(32);
  ClientManager manager(&bus, &busHandler, &req, &monitor, &reactor_queue);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[0], ebus::ClientType::regular);

  bus.start();
  manager.start();

  TestReactor reactor(bus, busHandler, &manager, nullptr, &reactor_queue);

  uint8_t addr = 0x33;
  send(sv[1], &addr, 1, 0);

  REQUIRE(reactor.waitFor([&] { return req.busRequestPending(); }));

  // Wait for session timeout (default 500ms) while keeping reactor alive
  platform::sleepMilli(600);

  REQUIRE(reactor.waitFor([&] { return !req.busRequestPending(); }));

  manager.stop();
  bus.stop();
  close(sv[1]);
}

TEST_CASE("ClientManager Client Removal") {
  Request req;
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  BusHandler busHandler(&req, nullptr);
  platform::Queue<ebus::OrchestrationEvent> reactor_queue(32);
  ClientManager manager(&bus, &busHandler, &req, &monitor, &reactor_queue);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[1], ebus::ClientType::regular);

  close(sv[0]);

  bus.writeByte(0xaa);
  manager.start();
  platform::sleepMilli(10);

  // Reaching here indicates manager handled closed socket without hang/crash
  CHECK_TEST("Manager handled closed socket", true);

  manager.stop();
  close(sv[1]);
}
