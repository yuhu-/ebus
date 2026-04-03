/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Platform/Queue.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_basic_operations() {
  std::cout << "\n=== Test: Basic Operations ===" << std::endl;
  ebus::Queue<int> q(5);

  run_test("Empty initially", q.size() == 0);

  q.push(1);
  q.push(2);
  run_test("Size after push", q.size() == 2);

  int val;
  bool success = q.try_pop(val);
  run_test("try_pop success", success && val == 1);

  q.pop(val);
  run_test("pop success", val == 2);
  run_test("Empty after pops", q.size() == 0);

  success = q.try_pop(val);
  run_test("try_pop on empty", !success);
}

void test_capacity() {
  std::cout << "\n=== Test: Capacity ===" << std::endl;
  ebus::Queue<int> q(2);  // Small capacity

  run_test("push 1", q.push(1));
  run_test("push 2", q.push(2));

  // Queue is full now
  bool pushed = q.try_push(3);
  run_test("try_push on full", !pushed);

  // Push with timeout should fail
  auto start = std::chrono::steady_clock::now();
  pushed = q.push(3, std::chrono::milliseconds(50));
  auto end = std::chrono::steady_clock::now();
  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                 .count();

  run_test("push timeout on full", !pushed);
  run_test("timeout duration approx 50ms", dur >= 40);
}

void test_blocking() {
  std::cout << "\n=== Test: Blocking Pop ===" << std::endl;
  ebus::Queue<int> q(5);

  std::thread producer([&q]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.push(42);
  });

  int val = 0;
  bool success = q.pop(val, std::chrono::milliseconds(200));

  producer.join();

  run_test("Blocking pop success", success && val == 42);
}

void test_multithreaded() {
  std::cout << "\n=== Test: Multi-threaded ===" << std::endl;
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

  if (sum_consumed != count) {
    std::cout << "  Expected: " << count << ", Got: " << sum_consumed
              << std::endl;
  }
  run_test("Consumed all items", sum_consumed == count);
  run_test("Queue empty", q.size() == 0);
}

void test_clear() {
  std::cout << "\n=== Test: Clear ===" << std::endl;
  ebus::Queue<int> q(5);
  q.push(1);
  q.push(2);
  run_test("Size before clear", q.size() == 2);
  q.clear();
  run_test("Size after clear", q.size() == 0);
  int val;
  run_test("Empty after clear", !q.try_pop(val));
}

void test_move_only() {
  std::cout << "\n=== Test: Move Only (std::unique_ptr) ===" << std::endl;
  ebus::Queue<std::unique_ptr<int>> q(5);

  q.push(std::make_unique<int>(10));
  run_test("try_push move success", q.try_push(std::make_unique<int>(20)));

  std::unique_ptr<int> ptr;
  q.pop(ptr);
  run_test("Pop unique_ptr 10", ptr && *ptr == 10);

  q.try_pop(ptr);
  run_test("Try pop unique_ptr 20", ptr && *ptr == 20);
}

int main() {
  test_basic_operations();
  test_capacity();
  test_blocking();
  test_multithreaded();
  test_clear();
  test_move_only();

  std::cout << "\nAll queue tests passed!" << std::endl;
  return 0;
}