/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus.hpp>
#include <ebus/utils.hpp>
#include <vector>

#include "core/bus_monitor.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/mutex.hpp"
#include "platform/system.hpp"
#include "test_helpers.hpp"

using namespace ebus::detail;

TEST_CASE("VirtualBus: Reaction Logic", "[app][virtualbus]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0x01};
  // Disable auto-SYN for logic tests to prevent history noise
  runtime.bus.syn_gen = false;

  BusMonitor monitor;
  Request request;
  platform::Bus bus(config, runtime, &request, &monitor);

  ebus::VirtualBus vb(bus);

  std::vector<uint8_t> bus_history;
  platform::Mutex history_mutex;  // Protects bus_history

  struct VirtualBusTestCallbacks {
    platform::Mutex& mutex;
    std::vector<uint8_t>& history;
    void onRead(const uint8_t& b) {
      platform::LockGuard<platform::Mutex> lock(mutex);
      history.push_back(b);
    }
  };
  VirtualBusTestCallbacks callbacks{history_mutex, bus_history};
  bus.addReadListener(
      Delegate<void(const uint8_t&)>::bind<VirtualBusTestCallbacks,
                                           &VirtualBusTestCallbacks::onRead>(
          &callbacks));

  bus.start();

  SECTION("Repeat count works correctly") {
    // Trigger on 0x11 -> Inject 0x22 (Repeat twice)
    ebus::VirtualBus::MockReaction r;
    r.trigger = ebus::Sequence({0x11});
    r.action = ebus::Sequence({0x22});
    r.repeat_count = 2;
    r.delay_ms = 0;
    vb.addMockReaction(r);

    // Write trigger 3 times
    bus.writeByte(0x11);
    platform::sleepMilli(20);
    bus.writeByte(0x11);
    platform::sleepMilli(20);
    bus.writeByte(0x11);
    platform::sleepMilli(20);

    // Wait for the background simulator thread to process
    REQUIRE(waitCondition(
        [&] {
          platform::LockGuard<platform::Mutex> lock(history_mutex);
          return bus_history.size() >= 5;
        },
        1000));

    platform::LockGuard<platform::Mutex> lock(history_mutex);
    std::vector<uint8_t> expected = {0x11, 0x22, 0x11, 0x22, 0x11};
    REQUIRE(bus_history == expected);
  }

  SECTION("Multiple matching reactions fire sequentially") {
    // Setup two reactions for the same trigger
    // First one fires 2 times, then the second one takes over
    ebus::VirtualBus::MockReaction r1;
    r1.trigger = ebus::Sequence({0x11});
    r1.action = ebus::Sequence({0xcc});
    r1.repeat_count = 2;
    r1.delay_ms = 0;
    vb.addMockReaction(r1);

    ebus::VirtualBus::MockReaction r2;
    r2.trigger = ebus::Sequence({0x11});
    r2.action = ebus::Sequence({0xbb});
    r2.repeat_count = 1;
    r2.delay_ms = 0;
    vb.addMockReaction(r2);

    // Write trigger 4 times
    bus.writeByte(0x11);  // Matches Reaction 1 -> CC
    platform::sleepMilli(20);
    bus.writeByte(0x11);  // Matches Reaction 1 -> CC
    platform::sleepMilli(20);
    bus.writeByte(0x11);  // Reaction 1 exhausted -> matches Reaction 2 -> BB
    platform::sleepMilli(20);
    bus.writeByte(0x11);  // Both exhausted -> nothing fires
    platform::sleepMilli(20);

    // Expected history: 11, CC, 11, CC, 11, BB, 11
    REQUIRE(waitCondition(
        [&] {
          platform::LockGuard<platform::Mutex> lock(history_mutex);
          return bus_history.size() >= 7;
        },
        1000));

    platform::LockGuard<platform::Mutex> lock(history_mutex);
    std::vector<uint8_t> expected = {0x11, 0xcc, 0x11, 0xcc, 0x11, 0xbb, 0x11};
    REQUIRE(bus_history == expected);
  }

  SECTION("Master Ack reaction includes SYN") {
    // This simulates our device sending a slave response, and the mock
    // master (simulator) acknowledging it.
    vb.addMasterAckReaction("013f", 1, 0);

    // Write the trigger sequence (Slave Part: NN DBx CRC)
    auto trigger = ebus::frameSlave(ebus::toVector("013f"));
    for (uint8_t b : trigger) bus.writeByte(b);

    // The simulator should inject ACK (00) AND SYN (AA)
    REQUIRE(waitCondition(
        [&] {
          platform::LockGuard<platform::Mutex> lock(history_mutex);
          return bus_history.size() >= trigger.size() + 2;
        },
        1000));

    platform::LockGuard<platform::Mutex> lock(history_mutex);
    // Final bytes must be ACK then SYN
    REQUIRE(bus_history[bus_history.size() - 2] == ebus::Symbols::ack);
    REQUIRE(bus_history[bus_history.size() - 1] == ebus::Symbols::syn);
  }

  SECTION("Removal by ID works correctly") {
    ebus::VirtualBus::MockReaction r;
    r.trigger = ebus::Sequence({0x22});
    r.action = ebus::Sequence({0x33});
    r.repeat_count = 1;
    uint32_t id = vb.addMockReaction(r);

    // Remove the reaction immediately using the returned ID
    vb.removeMockReaction(id);

    bus.writeByte(0x22);
    platform::sleepMilli(50);  // Allow time for a potential reaction

    platform::LockGuard<platform::Mutex> lock(history_mutex);
    // History should only contain the trigger (0x22), no action (0x33)
    std::vector<uint8_t> expected = {0x22};
    REQUIRE(bus_history == expected);
  }

  SECTION("Removal by Trigger works correctly") {
    ebus::VirtualBus::MockReaction r;
    r.trigger = ebus::Sequence({0x44});
    r.action = ebus::Sequence({0x55});
    vb.addMockReaction(r);

    // Remove all reactions matching this specific trigger sequence
    vb.removeMockReaction(ebus::Sequence({0x44}));

    bus.writeByte(0x44);
    platform::sleepMilli(50);

    platform::LockGuard<platform::Mutex> lock(history_mutex);
    // History should only contain the trigger (0x44), no action (0x55)
    std::vector<uint8_t> expected = {0x44};
    REQUIRE(bus_history == expected);
  }

  bus.stop();
}
