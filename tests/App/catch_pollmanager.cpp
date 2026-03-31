/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <vector>

#include "App/PollManager.hpp"

TEST_CASE("PollManager: Registration", "[app][pollmanager]") {
  ebus::PollManager pm;

  uint32_t id1 = pm.addPollItem(1, {0x01, 0x02}, std::chrono::seconds(5));
  uint32_t id2 = pm.addPollItem(2, {0x03, 0x04}, std::chrono::seconds(10));

  REQUIRE(id1 != id2);
  REQUIRE(pm.getDueItems().empty());
}

TEST_CASE("PollManager: Timing and Recurrence", "[app][pollmanager]") {
  ebus::PollManager pm;

  pm.addPollItem(5, {0xaa, 0xbb}, std::chrono::seconds(1));

  REQUIRE(pm.getDueItems().empty());

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto due = pm.getDueItems();
  REQUIRE(due.size() == 1);
  if (!due.empty()) {
    REQUIRE(due[0].message == std::vector<uint8_t>{0xaa, 0xbb});
    REQUIRE(due[0].priority == 5);
  }

  REQUIRE(pm.getDueItems().empty());

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  REQUIRE(pm.getDueItems().size() == 1);
}

TEST_CASE("PollManager: Removal", "[app][pollmanager]") {
  ebus::PollManager pm;

  uint32_t id = pm.addPollItem(1, {0xff}, std::chrono::seconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  REQUIRE(pm.getDueItems().size() == 1);

  pm.removePollItem(id);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  REQUIRE(pm.getDueItems().empty());
}