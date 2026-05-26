/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/controller.hpp>
#include <ebus/utils.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "platform/simulation/bus_simulator.hpp"
#include "test_utils.hpp"

using namespace ebus::detail;

TEST_CASE("Controller: Lifecycle and API", "[app][controller]") {
  // Configuration
  ebus::EbusConfig config;
  config.runtime.address = 0x31;  // Slave 0x36
  config.runtime.system_inquiry = false;
  config.runtime.system_response = false;
  config.runtime.device.scan_on_startup = false;
  config.runtime.bus.syn_gen = true;

  ebus::Controller controller(config);
  REQUIRE(controller.isConfigured());

  // Central dispatcher callback
  std::atomic<bool> telegramSeen{false};
  controller.setTelegramCallback(
      [&](const ebus::TelegramInfo& info) { telegramSeen = true; });

  // Start service
  REQUIRE(controller.start());
  // Wait for the controller to be fully ready instead of a fixed sleep
  REQUIRE((waitCondition([&] { return controller.isRunning(); }, 1000)));

  // Active messaging (enqueue)
  std::vector<uint8_t> msg = {0xfe, 0xb5, 0x05, 0x04, 0x27, 0x00, 0x2d, 0x00};
  std::atomic<bool> resultCallbackFired{false};

  controller.enqueue(1, msg, [&](const ebus::ResultInfo& info) {
    REQUIRE(info.success);
    resultCallbackFired = true;
  });

  // Replace 1s sleep with deterministic wait
  REQUIRE((waitCondition([&] { return resultCallbackFired.load(); }, 2000)));
  REQUIRE((waitCondition([&] { return telegramSeen.load(); }, 100)));

  // Metrics
  auto metrics = controller.getMetrics();
  REQUIRE(metrics.handler.messages_active == 1);

  // Shutdown
  controller.stop();
  REQUIRE(!controller.isRunning());
}

TEST_CASE("Controller: System Discovery automated response",
          "[app][controller]") {
  ebus::EbusConfig config;
  config.runtime.address = 0x01;
  config.runtime.lock_counter = 0;
  config.runtime.system_inquiry = false;
  config.runtime.system_response = true;
  config.runtime.device.scan_on_startup = false;
  config.runtime.bus.syn_gen = true;

  ebus::Controller controller(config);

  std::atomic<int> inquiryOfExistenceCount{0};
  std::atomic<int> signOfLifeCount{0};
  controller.setTelegramCallback([&](const ebus::TelegramInfo& info) {
    if (ebus::matches(info.master_view, ebus::Sequence::InquiryOfExistence(),
                      1)) {
      inquiryOfExistenceCount++;
    }

    if (ebus::matches(info.master_view, ebus::Sequence::SignOfLife(), 1)) {
      signOfLifeCount++;
    }
  });

  REQUIRE(controller.start());
  // Wait for the controller to be fully ready instead of a fixed sleep
  REQUIRE((waitCondition([&] { return controller.isRunning(); }, 1000)));

  // Setup a Peer Bus to simulate an external master (0x10).
  // We must use a unique address and disable SYN generation for the peer
  // to avoid collisions with the controller on the simulated wire.
  ebus::RuntimeConfig peerRuntime = config.runtime;
  peerRuntime.address = 0x10;
  peerRuntime.bus.syn_gen = false;

  Request peerReq;
  peerReq.setLockCounter(0);
  platform::Bus peerBus(config.bus, peerRuntime, &peerReq, nullptr);
  peerBus.start();
  BusSimulator peerSim(peerBus);

  // 1. Simulate an external "Inquiry of Existence" broadcast from master 0x10
  // We explicitly write the SYN to start the arbitration window.
  peerBus.writeByte(ebus::Symbols::syn);
  platform::sleepMicro(200);
  peerSim.injectMasterMessage(0x10, ebus::Sequence::InquiryOfExistence());

  // 2. The controller should see its own broadcast (echo), trigger the
  // discovery logic, and enqueue a Sign of Life (07 FF) response.
  REQUIRE((waitCondition([&] { return inquiryOfExistenceCount.load() == 1; },
                         3000)));
  REQUIRE((waitCondition([&] { return signOfLifeCount.load() == 1; }, 3000)));

  controller.stop();
  peerBus.stop();
}
