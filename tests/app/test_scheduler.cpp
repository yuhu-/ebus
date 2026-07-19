/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/data_types.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/simulation/bus_simulator.hpp"
#include "platform/system.hpp"

using namespace ebus::detail;

TEST_CASE("Scheduler: Broadcast Success (feb5050327002d)", "[app][scheduler]") {
  Request request;
  ebus::BusConfig config;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);
  BusHandler busHandler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&busHandler));

  BusSimulator simulator(bus);

  // BC Success: Broadcast to fe. No slave response.
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("feb5050327002d")),
       ebus::Sequence(), 0, 0});  // 0 is infinite

  Scheduler scheduler(&handler);
  scheduler.attachHandlerCallbacks();
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  scheduler.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) {
      last_error_session.store(info.session_id);
    } else if (info.message_type == ebus::MessageType::active) {
      last_success_session.store(info.session_id);
    }
  });

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("feb5050327002d"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         last_error_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(1)) {
    scheduler.tick();

    // Small sleep to prevent busy-waiting
    platform::sleepMilli(1);
  }

  REQUIRE(last_success_session.load() == session_id);

  bus.stop();
}

TEST_CASE("Scheduler: MS Success (52b509030d4600)", "[app][scheduler]") {
  Request request;
  ebus::BusConfig config;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);
  BusHandler busHandler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&busHandler));

  BusSimulator simulator(bus);

  // MS Success: Master to 52. Slave responds with payload 01 3f (plus ACK and
  // CRC).
  ebus::Sequence slavePart = ebus::frameSlave(ebus::toVector("013f"));
  ebus::Sequence fullSlaveResponse;
  fullSlaveResponse.push_back(ebus::Symbols::ack, false);
  fullSlaveResponse.append(slavePart);

  // Simulator needs to know about the full expected sequence including the
  // final SYN
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("52b509030d4600")),
       fullSlaveResponse, 1, 0});  // exactly 1 response

  Scheduler scheduler(&handler);
  scheduler.attachHandlerCallbacks();
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  scheduler.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) {
      last_error_session.store(info.session_id);
    } else if (info.message_type == ebus::MessageType::active) {
      last_success_session.store(info.session_id);
    }
  });

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("52b509030d4600"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         last_error_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(1)) {
    scheduler.tick();

    // Small sleep to prevent busy-waiting
    platform::sleepMilli(1);
  }

  REQUIRE(last_success_session.load() == session_id);

  bus.stop();
}

TEST_CASE("Scheduler: Retry Success (52b509030d4600)", "[app][scheduler]") {
  Request request;
  ebus::BusConfig config;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);
  BusHandler busHandler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&busHandler));

  BusSimulator simulator(bus);

  // Retry Success: Master to fe. Simulate NAK (ff) twice for protocol retry.
  // 3rd retry after backoff time will succeed with ACK (00) and payload 01 3f
  // (plus CRC).
  auto retry_trigger =
      ebus::frameMaster(source, ebus::toVector("52b509030d4600"));
  simulator.addMockReaction(
      {retry_trigger, ebus::Sequence({ebus::Symbols::nak}), 2, 0});

  ebus::Sequence retry_success_action;
  retry_success_action.push_back(ebus::Symbols::ack, false);
  retry_success_action.append(ebus::frameSlave(ebus::toVector("013f")));
  retry_success_action.push_back(ebus::Symbols::syn, false);

  simulator.addMockReaction(
      {retry_trigger, std::move(retry_success_action), 1, 0});

  Scheduler scheduler(&handler);
  scheduler.attachHandlerCallbacks();
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  scheduler.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) {
      last_error_session.store(info.session_id);
    } else if (info.message_type == ebus::MessageType::active) {
      last_success_session.store(info.session_id);
    }
  });

  bool test_passed = true;
  bool injected = false;

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("52b509030d4600"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(3)) {
    scheduler.tick();

    // Small sleep to prevent busy-waiting
    platform::sleepMilli(1);
  }

  REQUIRE(last_success_session.load() == session_id);

  bus.stop();
}
