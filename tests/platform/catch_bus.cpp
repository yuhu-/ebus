/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/utils.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/queue.hpp"
#include "platform/system.hpp"

using namespace ebus::detail;

// Helper to force a bus request for testing without waiting for lockCounter
static void force_request(Request& req, uint8_t addr) {
  req.setLockCounter(0);
  for (int i = 0; i < 30; ++i) {
    if (req.getLockCounter() == 0) break;
    req.run(ebus::Symbols::syn);
  }
  req.requestBus(addr);
}

TEST_CASE("Bus: Basic Communication", "[platform][bus]") {
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;

  Request req;
  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);

  bus.start();
  force_request(req, 0x03);
  auto* queue = bus.getQueue();

  std::vector<BusEvent> received;
  BusEvent ev;

  for (int i = 0; i < 2; ++i) {
    if (queue->pop(ev, std::chrono::milliseconds(100))) {
      received.push_back(ev);
    }
  }

  REQUIRE(received.size() >= 2);
  REQUIRE(received[0].byte == ebus::Symbols::syn);  // SYN
  REQUIRE(received[1].byte == 0x03);                // address

  // Ensure no premature SYN during traffic
  bool prematureSyn = false;
  for (int i = 0; i < 5; ++i) {
    bus.writeByte(0xff);
    platform::sleepMilli(20);
    BusEvent tempEv;
    while (queue->tryPop(tempEv)) {
      if (tempEv.byte == ebus::Symbols::syn) prematureSyn = true;
    }
  }
  REQUIRE(!prematureSyn);

  bus.stop();
}

TEST_CASE("Bus: SYN Timing", "[platform][bus]") {
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;

  Request req;
  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  auto* queue = bus.getQueue();

  auto start = std::chrono::steady_clock::now();
  bus.start();

  std::vector<std::chrono::steady_clock::time_point> timestamps;
  BusEvent ev;

  for (int i = 0; i < 4; ++i) {
    if (queue->pop(ev, std::chrono::milliseconds(200))) {
      if (ev.byte == ebus::Symbols::syn)
        timestamps.push_back(std::chrono::steady_clock::now());
    }
  }

  bus.stop();

  REQUIRE(timestamps.size() == 4);

  // Startup delay approx: syn_base + addr*10 + tolerance
  auto t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamps[0] - start)
                .count();
  INFO("Startup delay: " << t1 << " ms");
  REQUIRE(t1 >= 50);
  REQUIRE(t1 <= 90);

  auto t2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamps[1] - timestamps[0])
                .count();
  INFO("Interval 1-2: " << t2 << " ms");
  REQUIRE(t2 >= 40);
  REQUIRE(t2 <= 60);

  auto t3 = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamps[2] - timestamps[1])
                .count();
  INFO("Interval 2-3: " << t3 << " ms");
  REQUIRE(t3 >= 40);
  REQUIRE(t3 <= 60);
}

TEST_CASE("Bus: Raw Reception (Broadcast Simulation)", "[platform][bus]") {
  ebus::BusConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime = {.address = 0x01};
  Request req;
  BusMonitor monitor;
  platform::Bus bus(config, runtime, &req, &monitor);
  auto* queue = bus.getQueue();

  bus.start();

  std::vector<uint8_t> msg = ebus::toVector("10fe07000970160443183105052592");

  for (auto b : msg) {
    bus.writeByte(b);
    platform::sleepMilli(10);
  }

  std::vector<uint8_t> received;
  BusEvent ev;
  while (queue->pop(ev, std::chrono::milliseconds(50))) {
    received.push_back(ev.byte);
  }

  bus.stop();

  REQUIRE(received.size() == msg.size());
  REQUIRE(received == msg);
}