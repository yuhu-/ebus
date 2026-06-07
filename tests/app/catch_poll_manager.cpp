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

using namespace ebus::detail;

TEST_CASE("PollManager: Registration", "[app][pollmanager]") {
  PollManager pm;

  uint32_t id1 = pm.addPollItem(1, ebus::ByteView({0x01, 0x02}), 5000);
  uint32_t id2 = pm.addPollItem(2, ebus::ByteView({0x03, 0x04}), 10000);

  size_t count = 0;
  bool activity = false;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(id1 != id2);
  REQUIRE(count == 2);
}

TEST_CASE("PollManager: Timing and Recurrence", "[app][pollmanager]") {
  PollManager pm;

  pm.addPollItem(5, ebus::ByteView({0xaa, 0xbb}), 1000);

  size_t count = 0;
  bool activity = false;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 1);

  count = 0;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 0);

  platform::sleepMilli(1100);
  count = 0;
  pm.processDueItems(
      [&](const PollManager::Item& item) {
        count++;
        REQUIRE(item.message == ebus::Sequence({0xaa, 0xbb}));
        REQUIRE(item.priority == 5);
      },
      &activity);
  REQUIRE(count == 1);

  count = 0;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 0);

  platform::sleepMilli(1100);

  count = 0;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 1);
}

TEST_CASE("PollManager: Removal", "[app][pollmanager]") {
  PollManager pm;

  uint32_t id = pm.addPollItem(1, ebus::ByteView({0xff}), 1000);
  platform::sleepMilli(1100);

  size_t count = 0;
  bool activity = false;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 1);

  pm.removePollItem(id);
  platform::sleepMilli(1100);
  count = 0;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);
  REQUIRE(count == 0);
}

TEST_CASE("PollManager: Address Filtering", "[app][pollmanager]") {
  PollManager pm;

  // 1. Add items first while manager doesn't know its address yet
  pm.addPollItem(1, ebus::ByteView({0x08, 0x07}), 1000);  // External slave
  pm.addPollItem(1, ebus::ByteView({0x36, 0x07}), 1000);  // Our own slave

  // 2. Set address. This must trigger the purge of the 0x36 item.
  pm.setOwnAddress(0x31);

  size_t count = 0;
  bool activity = false;
  pm.processDueItems([&](const PollManager::Item&) { count++; }, &activity);

  REQUIRE(count == 1);  // Only the external one should remain
}

TEST_CASE("PollManager: mergeFromJson", "[app][pollmanager]") {
  PollManager pm;
  // Set address to 0x31 (Slave 0x36) to test self-polling filter
  pm.setOwnAddress(0x31);

  SECTION("Valid array of multiple items") {
    std::string json = R"([
      {"priority": 10, "message": "15070400", "interval_ms": 5000},
      {"priority": 2, "message": "05070400", "interval_ms": 1000}
    ])";

    REQUIRE(pm.mergeFromJson(json));
    REQUIRE(pm.getStatus().item_count == 2);
  }

  SECTION("Root is not an array should return false") {
    REQUIRE_FALSE(pm.mergeFromJson(R"({"priority": 10})"));
    REQUIRE(pm.getStatus().item_count == 0);
  }

  SECTION("Optional fields use sensible defaults") {
    // priority defaults to 5, interval_ms to 1000
    std::string json = R"([{"message": "15070400"}])";

    REQUIRE(pm.mergeFromJson(json));
    REQUIRE(pm.getStatus().item_count == 1);
  }

  SECTION("Self-polling messages are strictly filtered") {
    // Message starting with 0x36 (our slave address) should be rejected
    std::string json = R"([{"message": "36070400"}])";

    REQUIRE(pm.mergeFromJson(json));
    REQUIRE(pm.getStatus().item_count == 0);
  }

  SECTION("Items missing the message key are ignored") {
    std::string json = R"([
      {"priority": 10, "interval_ms": 5000}
    ])";

    REQUIRE(pm.mergeFromJson(json));
    REQUIRE(pm.getStatus().item_count == 0);
  }

  SECTION("Mixed valid and invalid types in array") {
    std::string json = R"([
      {"priority": 10, "message": "15070400"},
      "this_is_not_an_object",
      123,
      {"message": "05070400", "extra_key": "ignore_me"}
    ])";

    REQUIRE(pm.mergeFromJson(json));
    // Only the two valid objects should be added
    REQUIRE(pm.getStatus().item_count == 2);
  }

  SECTION("Empty array is valid but adds nothing") {
    REQUIRE(pm.mergeFromJson("[]"));
    REQUIRE(pm.getStatus().item_count == 0);
  }
}
