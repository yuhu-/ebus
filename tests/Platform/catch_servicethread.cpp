/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <atomic>
#include <chrono>
#include <thread>

#include "Platform/ServiceThread.hpp"

TEST_CASE("ServiceThread: Basic execution and join", "[platform][servicethread]") {
  std::atomic<int> counter{0};
  bool threadStarted = false;

  ebus::ServiceThread worker(
      "testThread",
      [&]() {
        threadStarted = true;
        counter++;
      },
      2048, 1);

  worker.start();
  worker.join();

  REQUIRE(threadStarted);
  REQUIRE(counter == 1);
}

TEST_CASE("ServiceThread: Destructor performs implicit join", "[platform][servicethread]") {
  std::atomic<int> counter{0};
  bool threadStarted = false;

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
    // Destructor should join when worker goes out of scope
  }

  REQUIRE(threadStarted);
  REQUIRE(counter == 1);
}