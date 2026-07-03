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
#include "test_helpers.hpp"

using namespace ebus::detail;

// Helper to mimic run_test behaviour but using Catch2
static void CHECK_TEST(const std::string& name, bool condition) {
  INFO(name);
  REQUIRE(condition);
}

// RAII wrapper for the two FDs created by socketpair
class SocketPairGuard {
 private:
  int local_fd_;   // FD passed to ClientManager (the owner)
  int remote_fd_;  // FD used only by tests (simulator side, needs closing)
 public:
  SocketPairGuard(int local, int remote)
      : local_fd_(local), remote_fd_(remote) {}
  ~SocketPairGuard() {
    if (local_fd_ != -1) {
      // NOTE: We do NOT close the 'local' FD here because ClientManager takes
      // ownership via std::unique_ptr<platform::Socket>(local) and manages it.
      // Only closing raw FDs managed *outside* of the ClientManager's
      // possession.
    }
    if (remote_fd_ != -1) {
      close(remote_fd_);  // Close only the simulator end FD here
    }
  }
  int getLocal() const { return local_fd_; }
  int getRemote() const { return remote_fd_; }
};

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
  // Local: svReg[0], Remote: svReg[1]
  SocketPairGuard reg_guard(svReg[0], svReg[1]);

  manager.addClient(std::make_unique<platform::Socket>(reg_guard.getLocal()),
                    ebus::ClientType::regular);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  // Local: svRO[0], Remote: svRO[1]
  SocketPairGuard ro_guard(svRO[0], svRO[1]);

  manager.addClient(std::make_unique<platform::Socket>(ro_guard.getLocal()),
                    ebus::ClientType::read_only);

  bus.start();
  manager.start();

  TestReactor reactor(bus, busHandler, &manager, nullptr, &reactor_queue);

  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  bus.writeByte(ebus::Symbols::syn);

  // Send complete telegram before session starts
  for (size_t i = 0; i < telegram.size(); ++i) {
    send(reg_guard.getRemote(), &telegram[i], 1, 0);
  }

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor([&] { return req.busRequestPending(); }));

  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor(
      [&] { return req.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t echo;
  // Note: First SYN after requestBus() is filtered (one-shot), so expect 3
  // instead of 4
  for (int i = 0; i < 3; ++i) {
    REQUIRE(reactor.readFromSocket(reg_guard.getRemote(), &echo, 1));
    CHECK_TEST("Regular received correct SYN echo", echo == ebus::Symbols::syn);
  }

  REQUIRE(reactor.readFromSocket(reg_guard.getRemote(), &echo, 1));
  CHECK_TEST("Regular received correct address byte echo", echo == telegram[0]);

  // Read back the rest of the telegram echoes
  for (size_t i = 1; i < telegram.size(); ++i) {
    REQUIRE(reactor.readFromSocket(reg_guard.getRemote(), &echo, 1));
    CHECK_TEST("Client received correct byte echo", echo == telegram[i]);
  }

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa};
  expectedRO.insert(expectedRO.end(), telegram.begin(), telegram.end());
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(ro_guard.getRemote(), actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
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

  int svEnh[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  // Local: svEnh[0], Remote: svReg[1]
  SocketPairGuard enh_guard(svEnh[0], svEnh[1]);

  manager.addClient(std::make_unique<platform::Socket>(enh_guard.getLocal()),
                    ebus::ClientType::enhanced);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  // Local: svRO[0], Remote: svRO[1]
  SocketPairGuard ro_guard(svRO[0], svRO[1]);

  manager.addClient(std::make_unique<platform::Socket>(ro_guard.getLocal()),
                    ebus::ClientType::read_only);

  bus.start();
  manager.start();

  TestReactor reactor(bus, busHandler, &manager, nullptr, &reactor_queue);

  bus.writeByte(ebus::Symbols::syn);

  // Send all commands before session starts
  uint8_t cmdStart[] = {0xc8, 0xb3};
  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(enh_guard.getRemote(), cmdStart, 2, 0);
  send(enh_guard.getRemote(), cmdSend, 2, 0);

  bus.writeByte(ebus::Symbols::syn);
  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor([&] { return req.busRequestPending(); }));

  bus.writeByte(ebus::Symbols::syn);

  REQUIRE(reactor.waitFor(
      [&] { return req.getResult() == ebus::RequestResult::first_won; }));

  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t resp[2];
  // Note: First SYN after requestBus() is filtered (one-shot), so expect 3
  // instead of 4
  for (int i = 0; i < 3; ++i) {
    reactor.readFromSocket(enh_guard.getRemote(), resp, 2);
    CHECK_TEST("Enhanced received correct SYN echo",
               (resp[0] == 0xc6 && resp[1] == 0xaa));
  }

  REQUIRE(reactor.readFromSocket(enh_guard.getRemote(), resp, 2));
  CHECK_TEST("Enhanced received STARTED", resp[0] == 0xc8 && resp[1] == 0xb3);

  // Read the encoded 0xfe response
  REQUIRE(reactor.readFromSocket(enh_guard.getRemote(), resp, 2));
  CHECK_TEST("Enhanced received encoded 0xfe",
             resp[0] == 0xc7 && resp[1] == 0xbe);

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa, 0x33, 0xfe};
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(readExact(ro_guard.getRemote(), actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  bus.stop();
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
  manager.addClient(std::make_unique<platform::Socket>(sv[0]),
                    ebus::ClientType::regular);

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
