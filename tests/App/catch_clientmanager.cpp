/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <ebus/Definitions.hpp>
#include <iostream>
#include <vector>

#include "App/ClientManager.hpp"
#include "Core/BusHandler.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "TestUtils.hpp"

// Helper to mimic run_test behaviour but using Catch2
static void CHECK_TEST(const std::string& name, bool condition) {
  INFO(name);
  REQUIRE(condition);
}

TEST_CASE("ClientManager Orchestration (Regular + ReadOnly)") {
  ebus::Request req;
  req.setMaxLockCounter(3);
  req.reset();

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = false};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int svReg[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  manager.addClient(svReg[0], ebus::ClientType::Regular);

  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  bus.start();
  busHandler.start();
  manager.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  send(svReg[1], &telegram[0], 1, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  CHECK_TEST("Arbitration resolved and won",
             req.getResult() == ebus::RequestResult::firstWon);
  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t echo;
  for (int i = 0; i < 3; ++i) read_exact(svReg[1], &echo, 1);

  for (size_t i = 1; i < telegram.size(); ++i) {
    send(svReg[1], &telegram[i], 1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(read_exact(svReg[1], &echo, 1));
    CHECK_TEST("Client received correct byte echo", echo == telegram[i]);
  }

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa};
  expectedRO.insert(expectedRO.end(), telegram.begin(), telegram.end());
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(read_exact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svReg[1]);
  close(svRO[1]);
}

TEST_CASE("ClientManager Enhanced Active Sending") {
  ebus::Request req;
  req.setMaxLockCounter(3);
  req.reset();

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0x01, .window = 50, .offset = 5};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int svEnh[2], svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);

  manager.addClient(svEnh[0], ebus::ClientType::Enhanced);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  bus.start();
  busHandler.start();
  manager.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  uint8_t cmdStart[] = {0xc8, 0xb3};
  send(svEnh[1], cmdStart, 2, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  CHECK_TEST("Arbitration resolved and won",
             req.getResult() == ebus::RequestResult::firstWon);
  CHECK_TEST("LockCounter reset to max", req.getLockCounter() == 3);

  uint8_t resp[2];
  for (int i = 0; i < 2; ++i) {
    read_exact(svEnh[1], resp, 2);
    CHECK_TEST("Enhanced received correct SYN echo",
               (resp[0] == 0xc6 && resp[1] == 0xaa));
  }

  REQUIRE(read_exact(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received RESP_STARTED",
             resp[0] == 0xc8 && resp[1] == 0xb3);

  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(svEnh[1], cmdSend, 2, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  REQUIRE(read_exact(svEnh[1], resp, 2));
  CHECK_TEST("Enhanced received encoded 0xfe",
             resp[0] == 0xc7 && resp[1] == 0xbe);

  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa, 0x33, 0xfe};
  std::vector<uint8_t> actualRO(expectedRO.size());
  REQUIRE(read_exact(svRO[1], actualRO.data(), actualRO.size()));
  CHECK_TEST("ReadOnly client received full trace", actualRO == expectedRO);

  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svEnh[1]);
  close(svRO[1]);
}

TEST_CASE("ClientManager Watchdog Timeout") {
  ebus::Request req;
  req.setMaxLockCounter(0);
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::BusHandler busHandler(&req, nullptr, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[0], ebus::ClientType::Regular);

  bus.start();
  manager.start();

  uint8_t addr = 0x33;
  send(sv[1], &addr, 1, 0);

  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }
  CHECK_TEST("Client is now active", req.busRequestPending());

  usleep(1100000);

  CHECK_TEST("Watchdog cleared active client", !req.busRequestPending());

  manager.stop();
  bus.stop();
  close(sv[1]);
}

TEST_CASE("Client Removal") {
  ebus::Request req;
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::BusHandler busHandler(&req, nullptr, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[1], ebus::ClientType::Regular);

  close(sv[0]);

  bus.writeByte(0xaa);
  manager.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Reaching here indicates manager handled closed socket without hang/crash
  CHECK_TEST("Manager handled closed socket", true);

  manager.stop();
  close(sv[1]);
}
