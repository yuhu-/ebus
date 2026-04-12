/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>

#include "core/request.hpp"
#include "utils/common.hpp"

TEST_CASE("Request: defaults", "[core][request]") {
  ebus::Request r;
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::observe);
  REQUIRE(r.getResult() == ebus::RequestResult::observeSyn);
}

TEST_CASE("Request: requestBus, completion and handler callback",
          "[core][request]") {
  ebus::Request r;
  r.setMaxLockCounter(0);
  r.reset();  // lockCounter == maxLockCounter (0)
  REQUIRE(r.busAvailable() == true);

  bool cb_called = false;
  r.setHandlerBusRequestedCallback([&] { cb_called = true; });

  bool requested = r.requestBus(0x33);
  REQUIRE(requested == true);
  REQUIRE(r.busRequestPending() == true);

  r.busRequestCompleted();
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::first);
  REQUIRE(cb_called == true);
}

TEST_CASE("Request: first -> firstWon flow", "[core][request]") {
  ebus::Request r;
  r.setMaxLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  r.busRequestCompleted();  // moves to first

  auto res = r.run(ebus::sym_syn);
  REQUIRE(res == ebus::RequestResult::firstSyn);

  res = r.run(0x33);  // our address observed -> won
  REQUIRE(res == ebus::RequestResult::firstWon);
  REQUIRE(r.getState() == ebus::RequestState::observe);
}

TEST_CASE("Request: first -> firstRetry -> retry -> second -> secondWon",
          "[core][request]") {
  ebus::Request r;
  r.setMaxLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  r.busRequestCompleted();  // state = first

  // observe SYN first
  auto res = r.run(ebus::sym_syn);
  REQUIRE(res == ebus::RequestResult::firstSyn);

  // simulate arbitration loss but same priority class (lower nibble match)
  // Use 0x13 (master-like address) which shares low nibble 0x3 with 0x33
  res = r.run(0x13);
  REQUIRE(res == ebus::RequestResult::firstRetry);
  REQUIRE(r.getState() == ebus::RequestState::retry);
  REQUIRE(r.busRequestPending() == true);  // re-armed

  // retry phase: see SYN -> move to second
  res = r.run(ebus::sym_syn);
  REQUIRE(res == ebus::RequestResult::retrySyn);
  REQUIRE(r.getState() == ebus::RequestState::second);

  // second-phase: observe our address -> won
  res = r.run(0x33);
  REQUIRE(res == ebus::RequestResult::secondWon);
  REQUIRE(r.getState() == ebus::RequestState::observe);
}

TEST_CASE("Request: startBit resets state and clears pending request",
          "[core][request]") {
  ebus::Request r;
  r.setMaxLockCounter(0);
  r.reset();
  r.requestBus(0x33);
  REQUIRE(r.busRequestPending() == true);
  r.startBit();
  REQUIRE(r.busRequestPending() == false);
  REQUIRE(r.getState() == ebus::RequestState::observe);
  REQUIRE(r.getResult() == ebus::RequestResult::observeSyn);
}
