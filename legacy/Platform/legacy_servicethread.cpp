/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "Platform/ServiceThread.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

int main() {
  std::cout << "--- Test: ServiceThread Lifecycle ---" << std::endl;

  std::atomic<int> counter{0};
  bool threadStarted = false;

  // 1. Test basic execution and join
  {
    ebus::ServiceThread worker(
        "testThread",
        [&]() {
          threadStarted = true;
          counter++;
        },
        2048, 1);

    worker.start();
    worker.join();  // Explicit join

    run_test("Thread function executed", threadStarted);
    run_test("Counter incremented", counter == 1);
  }

  // 2. Test implicit join via destructor
  threadStarted = false;
  {
    ebus::ServiceThread worker(
        "testDestructor",
        [&]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          threadStarted = true;
          counter++;
        },
        2048, 1);

    worker.start();
    // Destructor called here, should wait for thread to finish
  }

  run_test("Destructor performed implicit join", threadStarted);
  run_test("Counter incremented again", counter == 2);

  std::cout << "\nAll ServiceThread tests passed!" << std::endl;

  return 0;
}