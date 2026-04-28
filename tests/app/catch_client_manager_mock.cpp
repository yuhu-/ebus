/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/utils.hpp>
#include <memory>

#include "app/client_manager.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/system.hpp"
#include "test_utils.hpp"

using namespace ebus;
using namespace ebus::detail;

TEST_CASE("ClientManager: Mock Orchestration", "[app][clientmanager][mock]") {
  Request req;
  req.setLockCounter(0);
  req.reset();

  BusConfig config = {.device = "/dev/null", .simulate = true};
  RuntimeConfig runtime = {.address = 0x01};

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  Handler handler(runtime.address, &bus, &req, &monitor);
  BusHandler busHandler(&req, &handler, bus.getQueue());
  ClientManager manager(&bus, &busHandler, &req, &monitor);

  auto mockClient = std::make_shared<MockClient>(&req);
  manager.addClient(mockClient);

  bus.start();
  busHandler.start();
  manager.start();

  // Wait for background threads to stabilize
  REQUIRE(waitCondition([&] { return bus.getQueue() != nullptr; }));

  SECTION("Full bridge cycle: Request -> Arbitration -> Transmit") {
    // 1. Client wants to send an address (0x33)
    mockClient->pushInput(0x33);
    manager.wake();

    // Wait for Manager to see data and request bus
    REQUIRE(waitCondition([&] { return req.busRequestPending(); }));

    // 2. Simulate the Bus providing the SYN to start arbitration
    bus.writeByte(ebus::Symbols::syn);

    // Wait for FSM to move to 'first_won' and clear pending flag
    REQUIRE(waitCondition([&] { return !req.busRequestPending(); }));
    REQUIRE(req.getResult() == ebus::RequestResult::first_won);

    // 3. Client should have received the SYN echo and the address echo
    REQUIRE(waitCondition([&] { return !mockClient->getOutput().empty(); }));
    REQUIRE(mockClient->getOutput()[0] == ebus::Symbols::syn);
    REQUIRE(mockClient->getOutput()[1] == 0x33);

    REQUIRE(req.getState() == ebus::RequestState::observe);

    // 4. Continue sending the rest of the telegram (Broadcast to fe)
    std::vector<uint8_t> body = {0xfe, 0xb5, 0x05, 0x01, 0xec};
    for (auto b : body) {
      mockClient->pushInput(b);
      manager.wake();
    }

    REQUIRE(waitCondition([&] { return mockClient->getOutput().size() >= 7; }));
    REQUIRE(mockClient->getOutput().back() == 0xec);
  }

  SECTION("Outbound buffer overflow detection") {
    // 1. Setup client with very small buffer (2 bytes)
    auto smallClient = std::make_shared<MockClient>(&req, true, 2);
    manager.addClient(smallClient);
    REQUIRE(waitCondition([&] { return smallClient->isConnected(); }));

    // 2. Send bytes from bus. 1st byte (SYN) -> ok (buffer size 1)
    bus.writeByte(ebus::Symbols::syn);
    REQUIRE(
        waitCondition([&] { return smallClient->getOutput().size() == 1; }));
    REQUIRE(smallClient->isConnected());

    // 3. 2nd byte -> ok (buffer size 2)
    bus.writeByte(0x11);
    REQUIRE(
        waitCondition([&] { return smallClient->getOutput().size() == 2; }));
    REQUIRE(smallClient->isConnected());

    // 4. 3rd byte -> overflow -> stop() called
    bus.writeByte(0x22);
    REQUIRE(waitCondition([&] { return !smallClient->isConnected(); }));
  }

  manager.stop();
  busHandler.stop();
  bus.stop();
}
