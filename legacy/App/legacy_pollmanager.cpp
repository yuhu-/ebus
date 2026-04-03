/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include "App/PollManager.hpp"

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

  uint32_t id1 = pm.addPollItem(1, {0x01, 0x02}, std::chrono::seconds(5));
  uint32_t id2 = pm.addPollItem(2, {0x03, 0x04}, std::chrono::seconds(10));

  run_test("IDs are unique", id1 != id2);
  run_test("Initial due items list is empty", pm.getDueItems().empty());
}

void test_timing() {
  std::cout << "--- Test: Timing and Recurrence ---" << std::endl;
  ebus::PollManager pm;

  // Register item with 1 second interval
  pm.addPollItem(5, {0xAA, 0xBB}, std::chrono::seconds(1));

  // 1. Not due yet
  run_test("Not due after 0s", pm.getDueItems().empty());

  // 2. Wait for expiration
  std::cout << "  Waiting 1.1s for poll item..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto due = pm.getDueItems();
  run_test("Item due after 1.1s", due.size() == 1);
  if (!due.empty()) {
    run_test("Payload matches",
             due[0].message == std::vector<uint8_t>{0xAA, 0xBB});
    run_test("Priority matches", due[0].priority == 5);
  }

  // 3. Check recurrence
  run_test("Empty immediately after being popped", pm.getDueItems().empty());

  std::cout << "  Waiting another 1.1s..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  run_test("Item due again after second interval",
           pm.getDueItems().size() == 1);
}

void test_removal() {
  std::cout << "--- Test: Removal ---" << std::endl;
  ebus::PollManager pm;

  uint32_t id = pm.addPollItem(1, {0xFF}, std::chrono::seconds(1));
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // Verify it's ready to pop
  run_test("Item is due", pm.getDueItems().size() == 1);

  pm.removePollItem(id);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  run_test("Removed item does not appear in due list",
           pm.getDueItems().empty());
}

int main() {
  test_registration();
  test_timing();
  test_removal();

  std::cout << "\nAll PollManager tests passed!" << std::endl;

  return EXIT_SUCCESS;
}