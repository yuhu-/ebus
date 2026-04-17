/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <vector>

#include "app/poll_manager.hpp"

TEST_CASE("PollManager: Registration", "[app][pollmanager]") {
  ebus::PollManager pm;

  uint32_t id1 =
      pm.addPollItem(1, ebus::ByteView({0x01, 0x02}), std::chrono::seconds(5));
  uint32_t id2 =
      pm.addPollItem(2, ebus::ByteView({0x03, 0x04}), std::chrono::seconds(10));

  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(id1 != id2);
  REQUIRE(count == 0);
}

TEST_CASE("PollManager: Timing and Recurrence", "[app][pollmanager]") {
  ebus::PollManager pm;

  pm.addPollItem(5, ebus::ByteView({0xaa, 0xbb}), std::chrono::seconds(1));

  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(count == 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  count = 0;
  pm.processDueItems([&](const ebus::PollItem& item) {
    count++;
    REQUIRE(item.message == ebus::Sequence({0xaa, 0xbb}));
    REQUIRE(item.priority == 5);
  });
  REQUIRE(count == 1);

  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(count == 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(count == 1);
}

TEST_CASE("PollManager: Removal", "[app][pollmanager]") {
  ebus::PollManager pm;

  uint32_t id =
      pm.addPollItem(1, ebus::ByteView({0xff}), std::chrono::seconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(count == 1);

  pm.removePollItem(id);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  REQUIRE(count == 0);
}