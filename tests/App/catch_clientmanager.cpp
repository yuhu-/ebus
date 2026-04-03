/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <ebus/Definitions.hpp>
#include <vector>

#include "App/ClientManager.hpp"
#include "Core/BusHandler.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "TestUtils.hpp"
#include "Utils/Common.hpp"

TEST_CASE("ClientManager: System Orchestration", "[app][clientmanager]") {
  ebus::Request req;
  req.setMaxLockCounter(0);
  req.reset();

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = true};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  bus.start();
  busHandler.start();
  manager.start();

  SECTION("Client orchestration (Regular + ReadOnly)") {
    int svReg[2], svRO[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, svReg) == 0);
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, svRO) == 0);

    manager.addClient(svReg[0], ebus::ClientType::Regular);
    manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

    // Broadcast Telegram: [33] [fe] [b5] [05] [04] [27] [00] [2d] [00] [2c]
    std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                     0x27, 0x00, 0x2d, 0x00, 0x2c};

    // Send the master address from the client.
    // The Manager will see this, call requestBus, and wait for the next SYN.
    send(svReg[1], &telegram[0], 1, 0);

    // Wait for arbitration to complete naturally (SYN -> Send 0x33 -> Echo ->
    // Won) This is driven by the background SYN generator.
    bool won = false;
    for (int i = 0; i < 100 && !won; ++i) {
      if (handler.getState() == ebus::HandlerState::activeSendMaster) {
        won = true;
        break;
      }
      ebus::sleep_ms(10);
    }
    REQUIRE(won);

    // Send remaining bytes
    for (size_t i = 1; i < telegram.size(); ++i) {
      send(svReg[1], &telegram[i], 1, 0);
      ebus::sleep_ms(5);
    }

    uint8_t bufReg[10], bufRO[10];
    REQUIRE(read_exact(svReg[1], bufReg, 10));
    REQUIRE(std::vector<uint8_t>(bufReg, bufReg + 10) == telegram);

    REQUIRE(read_exact(svRO[1], bufRO, 10));
    REQUIRE(std::vector<uint8_t>(bufRO, bufRO + 10) == telegram);

    close(svReg[1]);
    close(svRO[1]);
  }

  SECTION("Arbitration lost behavior") {
    int svReg[2], svRO[2], svEnh[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
    socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
    socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);

    manager.addClient(svReg[0], ebus::ClientType::Regular);
    manager.addClient(svRO[0], ebus::ClientType::ReadOnly);
    manager.addClient(svEnh[0], ebus::ClientType::Enhanced);

    // Consume Enhanced greeting
    char greeting[64];
    REQUIRE(read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length()));

    // Regular Client tries to win the bus with 0x33
    // req.forceResultForTest(ebus::RequestResult::observeSyn);
    uint8_t myAddr = 0x33;
    send(svReg[1], &myAddr, 1, 0);

    // Wait for request
    while (!req.busRequestPending()) ebus::sleep_ms(5);

    // Simulate collision where 0xbb wins
    uint8_t winner = 0xbb;
    // req.forceResultForTest(ebus::RequestResult::firstLost);
    req.busRequestCompleted();
    bus.writeByte(winner);

    ebus::sleep_ms(10);
    // req.forceResultForTest(ebus::RequestResult::observeData);

    uint8_t buf;
    REQUIRE(read_exact(svReg[1], &buf, 1));
    REQUIRE(buf == winner);

    REQUIRE(read_exact(svRO[1], &buf, 1));
    REQUIRE(buf == winner);

    uint8_t bufEnh[2];
    REQUIRE(read_exact(svEnh[1], bufEnh, 2));
    REQUIRE(bufEnh[0] == 0xc6);  // RESP_RECEIVED
    REQUIRE(bufEnh[1] == winner);

    close(svReg[1]);
    close(svRO[1]);
    close(svEnh[1]);
  }

  SECTION("Client removal on disconnect") {
    int sv[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    manager.addClient(sv[1], ebus::ClientType::Regular);

    close(sv[0]);  // Remote hangup

    bus.writeByte(ebus::sym_syn);  // Trigger manager loop
    ebus::sleep_ms(20);

    // If we reach here without a crash, the cleanup loop worked
    SUCCEED("Manager handled closed socket");
    close(sv[1]);
  }

  SECTION("Enhanced protocol active sending") {
    int svEnh[2], svRO[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
    socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);

    manager.addClient(svEnh[0], ebus::ClientType::Enhanced);
    manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

    char greeting[64];
    REQUIRE(read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length()));

    // CMD_START(0x33) -> 0xc8 0xb3
    uint8_t cmdStart[] = {0xc8, 0xb3};
    send(svEnh[1], cmdStart, 2, 0);

    // Wait for natural arbitration win
    bool won = false;
    for (int i = 0; i < 100 && !won; ++i) {
      if (handler.getState() == ebus::HandlerState::activeSendMaster) {
        won = true;
        break;
      }
      ebus::sleep_ms(10);
    }
    REQUIRE(won);

    uint8_t resp[2];
    REQUIRE(read_exact(svEnh[1], resp, 2));
    REQUIRE(resp[0] == 0xc8);  // RESP_STARTED
    REQUIRE(resp[1] == 0xb3);

    // CMD_SEND(0xfe) -> 0xc7 0xbe
    uint8_t cmdSend[] = {0xc7, 0xbe};
    send(svEnh[1], cmdSend, 2, 0);
    ebus::sleep_ms(20);

    REQUIRE(read_exact(svEnh[1], resp, 2));
    REQUIRE(resp[0] == 0xc7);  // RESP_RECEIVED
    REQUIRE(resp[1] == 0xbe);

    uint8_t raw[2];
    REQUIRE(read_exact(svRO[1], raw, 2));
    REQUIRE(raw[0] == 0x33);
    REQUIRE(raw[1] == 0xfe);

    close(svEnh[1]);
    close(svRO[1]);
  }

  SECTION("Enhanced protocol arbitration lost") {
    int svEnh[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
    manager.addClient(svEnh[0], ebus::ClientType::Enhanced);

    char greeting[64];
    REQUIRE(read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length()));

    uint8_t cmdStart[] = {0xc8, 0xb3};
    // req.forceResultForTest(ebus::RequestResult::observeSyn);
    send(svEnh[1], cmdStart, 2, 0);

    while (!req.busRequestPending()) ebus::sleep_ms(5);

    // Force arbitration lost to master 0x09
    uint8_t winner = 0x09;
    // req.forceResultForTest(ebus::RequestResult::firstLost);
    req.busRequestCompleted();
    bus.writeByte(winner);
    ebus::sleep_ms(20);

    uint8_t resp[2];
    REQUIRE(read_exact(svEnh[1], resp, 2));
    REQUIRE(resp[0] == 0xe8);  // RESP_FAILED
    REQUIRE(resp[1] == 0x89);

    // Verify client stayed connected as observer
    bus.writeByte(ebus::sym_syn);
    ebus::sleep_ms(20);

    REQUIRE(read_exact(svEnh[1], resp, 2));
    REQUIRE(resp[0] == 0xc6);  // RESP_RECEIVED
    REQUIRE(resp[1] == ebus::sym_syn);

    close(svEnh[1]);
  }

  SECTION("Watchdog timeout") {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    manager.addClient(sv[0], ebus::ClientType::Regular);

    uint8_t addr = 0x33;
    // req.forceResultForTest(ebus::RequestResult::observeSyn);
    send(sv[1], &addr, 1, 0);

    while (!req.busRequestPending()) ebus::sleep_ms(5);
    REQUIRE(req.busRequestPending());

    // Wait for watchdog (> 1000ms)
    ebus::sleep_ms(1100);

    REQUIRE_FALSE(req.busRequestPending());
    close(sv[1]);
  }

  manager.stop();
  busHandler.stop();
  bus.stop();
}
