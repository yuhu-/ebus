/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include "app/poll_manager.hpp"

/**
 * Legacy-style test helper.
 */
void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_registration() {
  std::cout << "--- Test: Registration ---" << std::endl;
  ebus::PollManager pm;

  uint32_t id1 =
      pm.addPollItem(1, ebus::ByteView({0x01, 0x02}), std::chrono::seconds(5));
  uint32_t id2 =
      pm.addPollItem(2, ebus::ByteView({0x03, 0x04}), std::chrono::seconds(10));

  run_test("IDs are unique", id1 != id2);
  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Initial due items list is empty", count == 0);
}

void test_timing() {
  std::cout << "--- Test: Timing and Recurrence ---" << std::endl;
  ebus::PollManager pm;

  // Register item with 1 second interval
  pm.addPollItem(5, ebus::ByteView({0xaa, 0xbb}), std::chrono::seconds(1));

  // 1. Not due yet
  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Not due after 0s", count == 0);

  // 2. Wait for expiration
  std::cout << "  Waiting 1.1s for poll item..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  count = 0;
  pm.processDueItems([&](const ebus::PollItem& item) {
    count++;
    run_test("Payload matches", item.message == ebus::Sequence({0xaa, 0xbb}));
    run_test("Priority matches", item.priority == 5);
  });
  run_test("Item due after 1.1s", count == 1);

  // 3. Check recurrence
  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Empty immediately after being processed", count == 0);

  std::cout << "  Waiting another 1.1s..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Item due again after second interval", count == 1);
}

void test_removal() {
  std::cout << "--- Test: Removal ---" << std::endl;
  ebus::PollManager pm;

  uint32_t id =
      pm.addPollItem(1, ebus::ByteView({0xff}), std::chrono::seconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // Verify it's ready to pop
  size_t count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Item is due", count == 1);

  pm.removePollItem(id);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  count = 0;
  pm.processDueItems([&](const ebus::PollItem&) { count++; });
  run_test("Removed item does not appear in due list", count == 0);
}

int main() {
  test_registration();
  test_timing();
  test_removal();

  std::cout << "\nAll PollManager tests passed!" << std::endl;

  return EXIT_SUCCESS;
}