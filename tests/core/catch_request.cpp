/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/utils.hpp>

#include "core/request.hpp"

using namespace ebus::detail;

TEST_CASE("Request: defaults", "[core][request]") {
  Request r;
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::observe);
  REQUIRE(r.getResult() == ebus::RequestResult::observe_syn);
}

TEST_CASE("Request: requestBus, completion and handler callback",
          "[core][request]") {
  Request r;
  r.setLockCounter(0);
  r.reset();  // lockCounter == maxLockCounter (0)
  REQUIRE(r.busAvailable() == true);

  struct RequestTestCallbacks {
    bool cb_called = false;
    void onBusRequested() { cb_called = true; }
  };
  RequestTestCallbacks callbacks;

  r.setHandlerBusRequestedCallback(
      platform::Delegate<void()>::bind<RequestTestCallbacks,
                                       &RequestTestCallbacks::onBusRequested>(
          &callbacks));

  bool requested = r.requestBus(0x33);
  REQUIRE(requested == true);
  REQUIRE(r.busRequestPending() == true);

  r.busRequestCompleted();
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::first);
  REQUIRE(callbacks.cb_called == true);
}

TEST_CASE("Request: first -> firstWon flow", "[core][request]") {
  Request r;
  r.setLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  r.busRequestCompleted();  // moves to first

  auto res = r.run(ebus::Symbols::syn);
  REQUIRE(res == ebus::RequestResult::first_syn);

  res = r.run(0x33);  // our address observed -> won
  REQUIRE(res == ebus::RequestResult::first_won);
  REQUIRE(r.getState() == ebus::RequestState::observe);
}

TEST_CASE("Request: first -> firstRetry -> retry -> second -> secondWon",
          "[core][request]") {
  Request r;
  r.setLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  r.busRequestCompleted();  // state = first

  // observe SYN first
  auto res = r.run(ebus::Symbols::syn);
  REQUIRE(res == ebus::RequestResult::first_syn);

  // simulate arbitration loss but same priority class (lower nibble match)
  // Use 0x13 (master-like address) which shares low nibble 0x3 with 0x33
  res = r.run(0x13);
  REQUIRE(res == ebus::RequestResult::first_retry);
  REQUIRE(r.getState() == ebus::RequestState::retry);
  REQUIRE(r.busRequestPending() == true);  // re-armed

  // retry phase: see SYN -> move to second
  res = r.run(ebus::Symbols::syn);
  REQUIRE(res == ebus::RequestResult::retry_syn);
  REQUIRE(r.getState() == ebus::RequestState::second);

  // second-phase: observe our address -> won
  res = r.run(0x33);
  REQUIRE(res == ebus::RequestResult::second_won);
  REQUIRE(r.getState() == ebus::RequestState::observe);
}

TEST_CASE("Request: startBit resets state and clears pending request",
          "[core][request]") {
  Request r;
  r.setLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  REQUIRE(r.busRequestPending() == true);
  r.startBit();
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::observe);
  REQUIRE(r.getResult() == ebus::RequestResult::observe_syn);
}

TEST_CASE("Request: Legacy Edge Cases", "[core][request][legacy]") {
  Request r;
  r.setLockCounter(0);
  r.reset();

  SECTION("Wrong byte (Wire-AND violation)") {
    // 0x33 = 0011 0011. Reading 0x5c (0101 1100) means we read a '1' where we
    // sent a '0' (bit 4). This is electrically impossible on eBUS.
    r.requestBus(0x33);
    r.busRequestCompleted();

    r.run(ebus::Symbols::syn);
    auto res = r.run(0x5c);

    REQUIRE(res == ebus::RequestResult::first_error);
    REQUIRE(r.getState() == ebus::RequestState::observe);
  }

  SECTION("Priority fit with subsequent noise") {
    // We share priority class (0x3), so we enter retry...
    r.requestBus(0x33);
    r.busRequestCompleted();
    r.run(ebus::Symbols::syn);
    r.run(0x13);
    REQUIRE(r.getState() == ebus::RequestState::retry);

    // ...but the retry attempt sees noise (0xc5) instead of a SYN or address
    auto res = r.run(0xc5);
    REQUIRE(res == ebus::RequestResult::retry_error);
    REQUIRE(r.getState() == ebus::RequestState::observe);
  }

  SECTION("Sub-address arbitration loss") {
    // Test from legacy: We are 0x30, bus is 0x10. Same Prio Class (0).
    r.requestBus(0x30);
    r.busRequestCompleted();
    r.run(ebus::Symbols::syn);

    auto res = r.run(0x10);
    REQUIRE(res == ebus::RequestResult::first_retry);
    REQUIRE(r.getState() == ebus::RequestState::retry);
    REQUIRE(r.busRequestPending() == true);
  }
}
