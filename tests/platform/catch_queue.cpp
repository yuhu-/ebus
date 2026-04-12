/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "platform/queue.hpp"

TEST_CASE("Queue: Basic Operations", "[platform][queue]") {
  ebus::Queue<int> q(5);

  REQUIRE(q.size() == 0);

  q.push(1);
  q.push(2);
  REQUIRE(q.size() == 2);

  int val;
  bool success = q.try_pop(val);
  REQUIRE(success);
  REQUIRE(val == 1);

  q.pop(val);
  REQUIRE(val == 2);
  REQUIRE(q.size() == 0);

  success = q.try_pop(val);
  REQUIRE(!success);
}

TEST_CASE("Queue: Capacity", "[platform][queue]") {
  ebus::Queue<int> q(2);  // Small capacity

  REQUIRE(q.push(1));
  REQUIRE(q.push(2));

  bool pushed = q.try_push(3);
  REQUIRE(!pushed);

  auto start = std::chrono::steady_clock::now();
  pushed = q.push(3, std::chrono::milliseconds(50));
  auto end = std::chrono::steady_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                 .count();

  REQUIRE(!pushed);
  REQUIRE(dur >= 40);
}

TEST_CASE("Queue: Blocking Pop", "[platform][queue]") {
  ebus::Queue<int> q(5);

  std::thread producer([&q]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.push(42);
  });

  int val = 0;
  bool success = q.pop(val, std::chrono::milliseconds(200));

  producer.join();

  REQUIRE(success);
  REQUIRE(val == 42);
}

TEST_CASE("Queue: Multi-threaded", "[platform][queue]") {
  ebus::Queue<int> q(100);
  const int count = 1000;
  std::atomic<int> sum_consumed(0);

  std::thread consumer([&q, &sum_consumed, count]() {
    int val;
    for (int i = 0; i < count; ++i) {
      if (q.pop(val)) {
        sum_consumed += val;
      }
    }
  });

  std::thread producer([&q, count]() {
    for (int i = 0; i < count; ++i) {
      q.push(1);
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(sum_consumed == count);
  REQUIRE(q.size() == 0);
}

TEST_CASE("Queue: Clear", "[platform][queue]") {
  ebus::Queue<int> q(5);
  q.push(1);
  q.push(2);
  REQUIRE(q.size() == 2);
  q.clear();
  REQUIRE(q.size() == 0);
  int val;
  REQUIRE(!q.try_pop(val));
}

TEST_CASE("Queue: Move Only (std::unique_ptr)", "[platform][queue]") {
  ebus::Queue<std::unique_ptr<int>> q(5);

  q.push(std::make_unique<int>(10));
  REQUIRE(q.try_push(std::make_unique<int>(20)));

  std::unique_ptr<int> ptr;
  q.pop(ptr);
  REQUIRE(ptr);
  REQUIRE(*ptr == 10);

  q.try_pop(ptr);
  REQUIRE(ptr);
  REQUIRE(*ptr == 20);
}