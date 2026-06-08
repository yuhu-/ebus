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
  std::atomic<uint32_t> successSession{0};
  controller.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) return;
    telegramSeen = true;
    if (info.message_type == ebus::MessageType::active) {
      successSession.store(info.session_id);
    }
  });

  // Start service
  REQUIRE(controller.start());
  // Wait for the controller to be fully ready instead of a fixed sleep
  REQUIRE((waitCondition([&] { return controller.isRunning(); }, 1000)));

  // Active messaging (enqueue)
  std::vector<uint8_t> msg = {0xfe, 0xb5, 0x05, 0x04, 0x27, 0x00, 0x2d, 0x00};
  uint32_t session_id = controller.enqueue(1, msg);
  REQUIRE(session_id > 0);

  // Replace 1s sleep with deterministic wait
  REQUIRE((waitCondition([&] { return successSession.load() == session_id; },
                         2000)));
  REQUIRE((waitCondition([&] { return telegramSeen.load(); }, 100)));

  // Metrics
  controller.fetchMetrics(
      [](const ebus::Metrics& m) { REQUIRE(m.handler.messages_active == 1); });

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
  controller.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) return;
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

TEST_CASE("Controller Reactor: Enqueue Synchronization",
          "[app][controller][reactor]") {
  ebus::EbusConfig config;
  config.runtime.address = 0x10;
  config.runtime.bus.syn_gen = true;
  config.runtime.lock_counter = 0;

  ebus::Controller controller(config);
  auto& vbus = controller.getVirtualBus();

  REQUIRE(controller.start());

  std::atomic<uint32_t> success_session{0};
  controller.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) return;
    if (info.message_type == ebus::MessageType::active) {
      success_session.store(info.session_id);
    }
  });

  // Enqueue a simple Identification Request (07 04)
  std::vector<uint8_t> msg = {0x15, 0x07, 0x04, 0x00};
  uint32_t session_id = controller.enqueue(10, msg);
  REQUIRE(session_id > 0);

  // Inject a mock slave response
  vbus.addSlaveReaction(0x10, "15070400", "020102");

  // Wait for the result - ensures UserRequest -> Scheduler -> ProtocolResult
  // loop
  REQUIRE(ebus::detail::waitCondition(
      [&] { return success_session.load() == session_id; }, 2000));

  controller.stop();
}

TEST_CASE("Controller Reactor: Immediate Rejection",
          "[app][controller][reactor]") {
  ebus::EbusConfig config;
  config.runtime.address = 0x10;
  config.runtime.bus.syn_gen = true;

  ebus::Controller controller(config);
  REQUIRE(controller.start());

  std::atomic<bool> error_called{false};
  ebus::ProtocolError last_err = ebus::ProtocolError::none;

  controller.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (!info.is_error) return;
    last_err = info.protocol_error;
    error_called = true;
  });

  // Enqueue invalid 1-byte message (0xff) - needs explicit vector conversion
  controller.enqueue(10, std::vector<uint8_t>{0xff});

  // The Reactor must catch the immediate rejection from Scheduler::tick
  REQUIRE(
      ebus::detail::waitCondition([&] { return error_called.load(); }, 2000));
  REQUIRE(last_err == ebus::ProtocolError::invalid_message);

  controller.stop();
}

TEST_CASE("Controller Reactor: Drain Loop Burst",
          "[app][controller][reactor]") {
  ebus::EbusConfig config;
  config.runtime.address = 0x10;
  config.runtime.bus.syn_gen = true;

  ebus::Controller controller(config);
  auto& vbus = controller.getVirtualBus();

  REQUIRE(controller.start());

  std::atomic<int> telegram_count{0};
  controller.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (!info.is_error) telegram_count++;
  });

  // Rapid injection of 10 messages
  for (int i = 0; i < 10; ++i) {
    vbus.injectMasterMessage(0x03, "fe070000");
  }

  // Reactor must drain the queue and dispatch all callbacks
  REQUIRE(
      ebus::detail::waitCondition([&] { return telegram_count >= 10; }, 3000));

  controller.stop();
}