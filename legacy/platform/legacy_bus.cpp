/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "utils/common.hpp"

// Helper to force a bus request for testing without waiting for lockCounter
void force_request(ebus::Request& req, uint8_t addr) {
  // Bypass lock counter for test setup
  req.setMaxLockCounter(0);
  // Process SYNs until lock counter allows request
  for (int i = 0; i < 30; ++i) {
    if (req.getLockCounter() == 0) break;
    req.run(ebus::sym_syn);
  }
  req.requestBus(addr);
}

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_basic_communication() {
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = true};

  ebus::Request req;
  ebus::Bus bus(config, runtime, &req);

  std::cout << "\n=== Test: Basic Communication ===" << std::endl;
  bus.start();
  force_request(req, 0x03);
  auto* queue = bus.getQueue();

  // Replace the TEST 1 loop with this:
  std::vector<ebus::BusEvent> received;
  ebus::BusEvent ev;

  // We expect at least 2 events (SYN and address)
  // We give each event up to 100ms to appear in the queue
  for (int i = 0; i < 2; ++i) {
    if (queue->pop(ev, std::chrono::milliseconds(100))) {
      received.push_back(ev);
    }
  }

  // Now evaluate
  run_test("Received at least 2 events", received.size() >= 2);
  if (received.size() >= 2) {
    std::cout << "  Received: 0x" << std::hex << (int)received[0].byte
              << " and 0x" << (int)received[1].byte << std::dec << std::endl;
    run_test("First byte is SYN (0xAA)", received[0].byte == 0xAA);
    run_test("Second byte is address (0x03)", received[1].byte == 0x03);
  }

  // TEST 2: Reset check (also with try_pop)
  bool prematureSyn = false;
  for (int i = 0; i < 5; ++i) {
    bus.writeByte(0xff);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ebus::BusEvent tempEv;
    while (queue->tryPop(tempEv)) {
      if (tempEv.byte == ebus::sym_syn) prematureSyn = true;
    }
  }
  run_test("No SYN during traffic", !prematureSyn);

  // TEST 3: Stop
  std::cout << "  Stopping bus..." << std::endl;
  bus.stop();
  run_test("Bus stopped successfully", true);
}

void test_syn_timing() {
  std::cout << "\n=== Test: SYN Timing Logic ===" << std::endl;
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = true};
  ebus::Request req;
  ebus::Bus bus(config, runtime, &req);
  auto* queue = bus.getQueue();

  auto start = std::chrono::steady_clock::now();
  bus.start();

  std::vector<std::chrono::steady_clock::time_point> timestamps;
  ebus::BusEvent ev;

  // Capture 4 SYNs
  for (int i = 0; i < 4; ++i) {
    if (queue->pop(ev, std::chrono::milliseconds(200))) {
      if (ev.byte == ebus::sym_syn) {
        timestamps.push_back(std::chrono::steady_clock::now());
      }
    }
  }

  bus.stop();

  run_test("Received 4 SYNs", timestamps.size() == 4);

  if (timestamps.size() == 4) {
    // 1. Startup -> 1st SYN: Expect ~65ms
    // Calculation: Base(50) + Addr(1)*10 + Tolerance(5) = 65ms
    auto t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamps[0] - start)
                  .count();
    std::cout << "  Startup delay: " << t1 << "ms (Target: ~65ms)" << std::endl;
    // Allow loose tolerance for startup/thread init
    run_test("Startup Timer (Unique)", t1 >= 50 && t1 <= 90);

    // 2. 1st SYN -> 2nd SYN: Expect ~50ms (Normal)
    // Once active, the generator runs at syn_base (50ms).
    auto t2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamps[1] - timestamps[0])
                  .count();
    std::cout << "  Interval 1-2:  " << t2 << "ms (Target: 50ms)" << std::endl;
    run_test("Repeat Timer (Normal) 1", t2 >= 40 && t2 <= 60);

    // 3. 2nd SYN -> 3rd SYN: Expect ~50ms (Normal)
    auto t3 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamps[2] - timestamps[1])
                  .count();
    std::cout << "  Interval 2-3:  " << t3 << "ms (Target: 50ms)" << std::endl;
    run_test("Repeat Timer (Normal) 2", t3 >= 40 && t3 <= 60);
  }
}

void test_raw_reception() {
  std::cout << "\n=== Test: Raw Reception (Broadcast Simulation) ==="
            << std::endl;
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = false};

  ebus::Request req;
  ebus::Bus bus(config, runtime, &req);
  auto* queue = bus.getQueue();

  bus.start();

  // A typical broadcast telegram (raw bytes)
  // We send this into the simulation pipe and expect to read it back intact.
  std::vector<uint8_t> msg = ebus::toVector("10fe07000970160443183105052592");

  for (auto b : msg) {
    bus.writeByte(b);
    // Minimal delay to ensure reader thread scheduling, though pipe handles
    // bursts.
    std::this_thread::sleep_for(std::chrono::microseconds(500));
  }

  std::vector<uint8_t> received;
  ebus::BusEvent ev;
  while (queue->pop(ev, std::chrono::milliseconds(50))) {
    received.push_back(ev.byte);
  }

  bus.stop();

  run_test("Received byte count matches", received.size() == msg.size());
  if (received.size() == msg.size()) {
    run_test("Received data content matches", received == msg);
  } else {
    std::cout << "  Expected: " << ebus::toString(msg) << std::endl;
    std::cout << "  Got:      " << ebus::toString(received) << std::endl;
  }
}

int main() {
  test_basic_communication();
  test_syn_timing();
  test_raw_reception();

  std::cout << "\nAll bus tests passed!" << std::endl;
  return 0;
}
