/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

#include "platform/service_thread.hpp"
#include "platform/system.hpp"

using namespace ebus::detail;

TEST_CASE("ServiceThread: Basic execution and join",
          "[platform][servicethread]") {
  std::atomic<int> counter{0};
  bool threadStarted = false;

  ServiceThread worker(
      "testThread",
      [&]() {
        threadStarted = true;
        counter++;
      },
      OrchestrationLimits::stack_size, OrchestrationLimits::priority_low);

  worker.start();
  worker.join();

  REQUIRE(threadStarted);
  REQUIRE(counter == 1);
}

TEST_CASE("ServiceThread: Destructor performs implicit join",
          "[platform][servicethread]") {
  std::atomic<int> counter{0};
  bool threadStarted = false;

  {
    ServiceThread worker(
        "testDestructor",
        [&]() {
          sleepMilli(50);
          threadStarted = true;
          counter++;
        },
        OrchestrationLimits::stack_size, OrchestrationLimits::priority_low);

    worker.start();
    // Destructor should join when worker goes out of scope
  }

  REQUIRE(threadStarted);
  REQUIRE(counter == 1);
}