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

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

  BusSimulator simulator(bus);

  // BC Success: Broadcast to fe. No slave response.
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("feb5050327002d")),
       ebus::Sequence(), 0, 0});  // 0 is infinite

  Scheduler scheduler(&handler);
  scheduler.attachHandlerCallbacks();
  scheduler.setMaxAttempts(3);
  scheduler.setBaseBackoff(50);

  ProtocolEvent received_ev{};
  scheduler.setProtocolEventSink(
      [&](ProtocolEvent&& ev) { received_ev = std::move(ev); });

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("feb5050327002d"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         last_error_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(1)) {
    scheduler.tick();

    // Process queued protocol events
    ProtocolEvent ev;
    if (received_ev.session_id != 0) {
      ev = std::move(received_ev);
      received_ev = ProtocolEvent{};
      scheduler.injectProtocolEvent(ev);
      if (ev.type == ProtocolEvent::Type::error) {
        last_error_session.store(ev.session_id);
      } else if (ev.type == ProtocolEvent::Type::telegram &&
                 ev.message_type == ebus::MessageType::active) {
        last_success_session.store(ev.session_id);
      }
    }

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

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

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
  scheduler.setMaxAttempts(3);
  scheduler.setBaseBackoff(50);

  ProtocolEvent received_ev{};
  scheduler.setProtocolEventSink(
      [&](ProtocolEvent&& ev) { received_ev = std::move(ev); });

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("52b509030d4600"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         last_error_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(1)) {
    scheduler.tick();

    // Process queued protocol events
    ProtocolEvent ev;
    if (received_ev.session_id != 0) {
      ev = std::move(received_ev);
      received_ev = ProtocolEvent{};
      scheduler.injectProtocolEvent(ev);
      if (ev.type == ProtocolEvent::Type::error) {
        last_error_session.store(ev.session_id);
      } else if (ev.type == ProtocolEvent::Type::telegram &&
                 ev.message_type == ebus::MessageType::active) {
        last_success_session.store(ev.session_id);
      }
    }

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

  BusMonitor bus_monitor;
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);
  BusHandler bus_handler(&request, &handler);

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

  // Bridge Physical Bus Events -> BusHandler
  bus.addBusEventListener(Delegate<void(const BusEvent& event)>::bind<
                          BusHandler, &BusHandler::onBusEvent>(&bus_handler));

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
  scheduler.setMaxAttempts(3);
  scheduler.setBaseBackoff(50);

  ProtocolEvent received_ev{};
  scheduler.setProtocolEventSink(
      [&](ProtocolEvent&& ev) { received_ev = std::move(ev); });

  bus.start();

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  uint32_t session_id = scheduler.enqueue(1, ebus::toVector("52b509030d4600"));
  REQUIRE(session_id > 0);

  auto test_start = ebus::Clock::now();
  while (last_success_session.load() != session_id &&
         (ebus::Clock::now() - test_start) < std::chrono::seconds(3)) {
    scheduler.tick();

    // Process queued protocol events
    ProtocolEvent ev;
    if (received_ev.session_id != 0) {
      ev = std::move(received_ev);
      received_ev = ProtocolEvent{};
      scheduler.injectProtocolEvent(ev);
      if (ev.type == ProtocolEvent::Type::error) {
        last_error_session.store(ev.session_id);
      } else if (ev.type == ProtocolEvent::Type::telegram &&
                 ev.message_type == ebus::MessageType::active) {
        last_success_session.store(ev.session_id);
      }
    }

    // Small sleep to prevent busy-waiting
    platform::sleepMilli(1);
  }

  REQUIRE(last_success_session.load() == session_id);

  bus.stop();
}

namespace {

struct BreakerHarness {
  ebus::RuntimeConfig runtime;
  Request request;
  BusMonitor bus_monitor;
  platform::Bus bus;
  Handler handler;
  Scheduler scheduler;

  BreakerHarness()
      : bus(config(), runtimeConfig(), &request, &bus_monitor),
        handler(runtimeConfig().address, &bus, &request, &bus_monitor),
        scheduler(&handler) {
    handler.setSourceAddress(runtimeConfig().address);
    scheduler.setMaxAttempts(1);  // production: single shot, next on schedule
    scheduler.setBreakerThreshold(3);
    scheduler.setBreakerCooldownMs(60000, 60000);
  }

  static ebus::BusConfig config() { return ebus::BusConfig{}; }
  static ebus::RuntimeConfig runtimeConfig() {
    ebus::RuntimeConfig r;
    r.address = 0x01;
    r.bus.syn_gen = true;
    return r;
  }

  uint32_t startItem() {
    uint32_t sid = scheduler.enqueue(1, ebus::toVector("feb5050327002d"));
    REQUIRE(sid > 0);
    REQUIRE(scheduler.tick() == true);
    return sid;
  }

  // Injects a terminal outcome AND settles the bus side. In production the
  // handler clears active_message_ via its own FSM as bus events flow;
  // without a running bus the test must do it explicitly (mirrors the
  // timeout path, which calls handler.reset()).
  bool settle(const ProtocolEvent& ev) {
    bool r = scheduler.injectProtocolEvent(ev);
    handler.reset();
    return r;
  }

  ProtocolEvent errorFor(uint32_t sid) {
    ProtocolEvent ev{};
    ev.type = ProtocolEvent::Type::error;
    ev.session_id = sid;
    ev.protocol_error = ebus::ProtocolError::error_active_master_echo;
    ev.result = ebus::RequestResult::observe_data;
    ev.level = ebus::LogLevel::error;
    return ev;
  }

  ProtocolEvent telegramFor(uint32_t sid) {
    ProtocolEvent ev{};
    ev.type = ProtocolEvent::Type::telegram;
    ev.session_id = sid;
    ev.message_type = ebus::MessageType::active;
    ev.telegram_type = ebus::TelegramType::broadcast;
    ev.level = ebus::LogLevel::info;
    return ev;
  }
};

}  // namespace

TEST_CASE("Scheduler breaker: trips after threshold and quarantines",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  REQUIRE(h.scheduler.breakerOpen() == false);

  for (int i = 0; i < 3; ++i) {
    uint32_t sid = h.startItem();
    REQUIRE(h.settle(h.errorFor(sid)) == true);
  }
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 3);
  REQUIRE(h.scheduler.breakerTrips() == 1);
  REQUIRE(h.scheduler.breakerOpen() == true);

  // Quarantined: tick must not start anything new.
  uint32_t sid = h.scheduler.enqueue(1, ebus::toVector("feb5050327002d"));
  REQUIRE(h.scheduler.tick() == false);
  REQUIRE(h.settle(h.errorFor(sid)) == false);

  // Mirror visible in /metrics/lib (SystemMetrics::scheduler).
  h.bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.scheduler.breaker_open == true);
    REQUIRE(m.scheduler.breaker_trips == 1);
    REQUIRE(m.scheduler.consecutive_failures == 3);
  });
}

TEST_CASE("Scheduler breaker: success resets the count",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  for (int i = 0; i < 2; ++i) {
    uint32_t sid = h.startItem();
    REQUIRE(h.settle(h.errorFor(sid)) == true);
  }
  REQUIRE(h.scheduler.breakerOpen() == false);

  uint32_t sid = h.startItem();
  REQUIRE(h.settle(h.telegramFor(sid)) == true);
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);

  // Two more failures stay below threshold: still closed.
  for (int i = 0; i < 2; ++i) {
    sid = h.startItem();
    REQUIRE(h.settle(h.errorFor(sid)) == true);
  }
  REQUIRE(h.scheduler.breakerOpen() == false);
}

TEST_CASE("Scheduler breaker: fatal errors do not blame the bus",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  for (int i = 0; i < 5; ++i) {
    uint32_t sid = h.startItem();
    ProtocolEvent ev = h.errorFor(sid);
    ev.protocol_error = ebus::ProtocolError::invalid_message;
    REQUIRE(h.settle(ev) == true);
  }
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);
  REQUIRE(h.scheduler.breakerOpen() == false);
}

TEST_CASE("Scheduler breaker: probe re-trips with doubling cooldown",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  h.scheduler.setBreakerThreshold(2);
  h.scheduler.setBreakerCooldownMs(50, 10000);

  for (int i = 0; i < 2; ++i) {
    uint32_t sid = h.startItem();
    REQUIRE(h.settle(h.errorFor(sid)) == true);
  }
  REQUIRE(h.scheduler.breakerOpen() == true);
  REQUIRE(h.scheduler.breakerTrips() == 1);

  // Let the quarantine expire: next tick is the single probe.
  std::this_thread::sleep_for(std::chrono::milliseconds(70));
  REQUIRE(h.scheduler.breakerOpen() == false);
  uint32_t sid = h.startItem();
  REQUIRE(h.settle(h.errorFor(sid)) == true);
  REQUIRE(h.scheduler.breakerTrips() == 2);
  REQUIRE(h.scheduler.breakerOpen() == true);

  // Manual reset re-arms immediately.
  h.scheduler.resetBreaker();
  REQUIRE(h.scheduler.breakerOpen() == false);
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);
  sid = h.startItem();
  REQUIRE(h.settle(h.telegramFor(sid)) == true);
}
