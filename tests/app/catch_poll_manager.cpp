/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <vector>

#include "app/poll_manager.hpp"
#include "platform/system.hpp"

using namespace ebus;
using namespace ebus::detail;

TEST_CASE("PollManager: Registration", "[app][pollmanager]") {
  PollManager pm;

  uint32_t id1 = pm.addPollItem(1, ByteView({0x01, 0x02}), 5000);
  uint32_t id2 = pm.addPollItem(2, ByteView({0x03, 0x04}), 10000);

  size_t count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(id1 != id2);
  REQUIRE(count == 2);
}

TEST_CASE("PollManager: Timing and Recurrence", "[app][pollmanager]") {
  PollManager pm;

  pm.addPollItem(5, ByteView({0xaa, 0xbb}), 1000);

  size_t count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 1);

  count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 0);

  platform::sleepMilli(1100);
  count = 0;
  pm.processDueItems([&](const PollItem& item) {
    count++;
    REQUIRE(item.message == Sequence({0xaa, 0xbb}));
    REQUIRE(item.priority == 5);
  });
  REQUIRE(count == 1);

  count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 0);

  platform::sleepMilli(1100);

  count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 1);
}

TEST_CASE("PollManager: Removal", "[app][pollmanager]") {
  PollManager pm;

  uint32_t id = pm.addPollItem(1, ::ByteView({0xff}), 1000);
  platform::sleepMilli(1100);

  size_t count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 1);

  pm.removePollItem(id);
  platform::sleepMilli(1100);
  count = 0;
  pm.processDueItems([&](const PollItem&) { count++; });
  REQUIRE(count == 0);
}