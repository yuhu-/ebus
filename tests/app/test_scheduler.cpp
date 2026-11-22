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
    scheduler.setMaxAttempts(1);  // Exercise terminal outcomes without retries.
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

TEST_CASE("Scheduler breaker: peer silence does not trip the bus",
          "[app][scheduler][breaker]") {
  // Scan probes and dead-register polls: master TX echo-verified, peer
  // never answers. Must never quarantine healthy bus traffic.
  BreakerHarness h;
  for (int i = 0; i < 5; ++i) {
    uint32_t sid = h.startItem();
    ProtocolEvent ev = h.errorFor(sid);
    ev.protocol_error = ebus::ProtocolError::error_active_master_ack;
    REQUIRE(h.settle(ev) == true);
  }
  for (int i = 0; i < 5; ++i) {
    uint32_t sid = h.startItem();
    ProtocolEvent ev = h.errorFor(sid);
    ev.protocol_error = ebus::ProtocolError::total_transfer_timeout;
    REQUIRE(h.settle(ev) == true);
  }
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);
  REQUIRE(h.scheduler.breakerTrips() == 0);
  REQUIRE(h.scheduler.breakerOpen() == false);

  // Bus-level faults still count: three echo mismatches trip.
  for (int i = 0; i < 3; ++i) {
    uint32_t sid = h.startItem();
    REQUIRE(h.settle(h.errorFor(sid)) == true);
  }
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 3);
  REQUIRE(h.scheduler.breakerTrips() == 1);
  REQUIRE(h.scheduler.breakerOpen() == true);
}

TEST_CASE("Scheduler breaker: lost arbitration never counts",
          "[app][scheduler][breaker]") {
  // Someone else transmitting proves the wire works: losing arbitration
  // must not feed the quarantine, however often it happens in a row
  // (busy-bus scan probes hit exactly this).
  BreakerHarness h;
  for (int i = 0; i < 5; ++i) {
    uint32_t sid = h.startItem();
    ProtocolEvent ev{};
    ev.type = ProtocolEvent::Type::lost;
    ev.session_id = sid;
    ev.level = ebus::LogLevel::info;
    REQUIRE(h.settle(ev) == true);
  }
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);
  REQUIRE(h.scheduler.breakerOpen() == false);

  h.bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.scheduler.fail_lost == 5);
    REQUIRE(m.scheduler.fail_bus == 0);
  });
}

TEST_CASE("Scheduler breaker: cause breakdown classifies outcomes",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  // 1 bus fault (echo), 1 silence (master_ack), 1 fatal (invalid_message).
  uint32_t sid = h.startItem();
  REQUIRE(h.settle(h.errorFor(sid)) == true);

  sid = h.startItem();
  ProtocolEvent silent{};
  silent.type = ProtocolEvent::Type::error;
  silent.session_id = sid;
  silent.protocol_error = ebus::ProtocolError::error_active_master_ack;
  silent.level = ebus::LogLevel::error;
  REQUIRE(h.settle(silent) == true);

  sid = h.startItem();
  ProtocolEvent fatal{};
  fatal.type = ProtocolEvent::Type::error;
  fatal.session_id = sid;
  fatal.protocol_error = ebus::ProtocolError::invalid_message;
  fatal.level = ebus::LogLevel::error;
  REQUIRE(h.settle(fatal) == true);

  h.bus_monitor.fetchMetrics([&](const ebus::Metrics& m) {
    REQUIRE(m.scheduler.fail_bus == 1);
    REQUIRE(m.scheduler.fail_silent == 1);
    REQUIRE(m.scheduler.fail_fatal == 1);
    REQUIRE(m.scheduler.fail_lost == 0);
  });
  // Single bus fault below threshold: still closed.
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
  REQUIRE(h.scheduler.injectProtocolEvent(h.telegramFor(sid)) == true);
}

TEST_CASE("Scheduler breaker: success restarts cooldown growth",
          "[app][scheduler][breaker]") {
  BreakerHarness h;
  h.scheduler.setBreakerThreshold(1);
  h.scheduler.setBreakerCooldownMs(50, 10000);

  // Trip, recover via success, trip again: second episode reopens at
  // base instead of inheriting the earlier peak (headless operation
  // has no user pressing re-arm).
  uint32_t sid = h.startItem();
  REQUIRE(h.settle(h.errorFor(sid)) == true);
  REQUIRE(h.scheduler.breakerTrips() == 1);
  // Quarantine first: the success below must arrive as a probe.
  std::this_thread::sleep_for(std::chrono::milliseconds(70));
  sid = h.startItem();
  REQUIRE(h.settle(h.telegramFor(sid)) == true);
  REQUIRE(h.scheduler.breakerTrips() == 0);
  REQUIRE(h.scheduler.breakerConsecutiveFailures() == 0);
  sid = h.startItem();
  REQUIRE(h.settle(h.errorFor(sid)) == true);
  REQUIRE(h.scheduler.breakerTrips() == 1);
  REQUIRE(h.scheduler.breakerOpen() == true);
}

TEST_CASE("Scheduler reports one terminal arbitration error after three losses",
          "[app][scheduler][retries]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  h.scheduler.setBaseBackoff(1);
  std::vector<ProtocolEvent> results;
  h.scheduler.setProtocolEventSink([&](ProtocolEvent&& event) {
    results.push_back(std::move(event));
  });
  const auto sid = h.startItem();
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = sid;
  for (unsigned attempt = 1; attempt <= 3; ++attempt) {
    REQUIRE(h.settle(lost));
    if (attempt < 3) {
      REQUIRE(results.empty());
      std::this_thread::sleep_until(h.scheduler.nextDueTime());
      REQUIRE(h.scheduler.tick());
    }
  }
  REQUIRE(results.size() == 1);
  const auto terminal = results.front();
  REQUIRE(terminal.type == ProtocolEvent::Type::error);
  REQUIRE(terminal.protocol_error == ebus::ProtocolError::arbitration_lost);
  REQUIRE(terminal.session_id == sid);
  REQUIRE(terminal.attempts == 3);
  REQUIRE(terminal.master[0] == 0x01);
  REQUIRE(terminal.master[1] == 0xfe);
  REQUIRE_FALSE(h.scheduler.injectProtocolEvent(terminal));
  REQUIRE(results.size() == 1);
  REQUIRE_FALSE(h.scheduler.breakerOpen());
  REQUIRE(h.scheduler.size() == 0);
}

TEST_CASE("Scheduler succeeds after a lost arbitration without a terminal error",
          "[app][scheduler][retries]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  h.scheduler.setBaseBackoff(1);
  std::vector<ProtocolEvent> results;
  h.scheduler.setProtocolEventSink([&](ProtocolEvent&& event) {
    results.push_back(std::move(event));
  });
  const auto sid = h.startItem();
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = sid;
  REQUIRE(h.settle(lost));
  std::this_thread::sleep_until(h.scheduler.nextDueTime());
  REQUIRE(h.scheduler.tick());
  REQUIRE(h.settle(h.telegramFor(sid)));
  REQUIRE(results.empty());
  REQUIRE(h.scheduler.size() == 0);
}

TEST_CASE("Scheduler reserves capacity for retrying its active telegram",
          "[app][scheduler][retries]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  h.scheduler.setBaseBackoff(1);
  const auto sid = h.startItem();
  REQUIRE(h.scheduler.size() == 1);
  const auto message = ebus::toVector("feb5050327002d");
  for (size_t i = 1; i < h.scheduler.capacity(); ++i) {
    REQUIRE(h.scheduler.enqueue(1, message) != 0);
  }
  REQUIRE(h.scheduler.enqueue(1, message) == 0);
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = sid;
  REQUIRE(h.settle(lost));
  REQUIRE(h.scheduler.size() == h.scheduler.capacity());
}

TEST_CASE("Scheduler cancellation removes only the requested queued session",
          "[app][scheduler][cancel]") {
  BreakerHarness h;
  const auto message = ebus::toVector("feb5050327002d");
  const auto first = h.scheduler.enqueue(1, message);
  const auto removed = h.scheduler.enqueue(2, message);
  const auto last = h.scheduler.enqueue(3, message);
  REQUIRE(h.scheduler.cancel(removed));
  REQUIRE(h.scheduler.size() == 2);
  REQUIRE_FALSE(h.scheduler.cancel(removed));
  REQUIRE_FALSE(h.scheduler.cancel(0));
  REQUIRE(h.scheduler.cancel(first));
  REQUIRE(h.scheduler.cancel(last));
  REQUIRE(h.scheduler.size() == 0);
  REQUIRE_FALSE(h.scheduler.tick());
}

TEST_CASE("Scheduler cancellation removes a retry waiting for backoff",
          "[app][scheduler][cancel]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  const auto sid = h.startItem();
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = sid;
  REQUIRE(h.settle(lost));
  REQUIRE(h.scheduler.size() == 1);
  REQUIRE(h.scheduler.cancel(sid));
  REQUIRE(h.scheduler.size() == 0);
  REQUIRE_FALSE(h.scheduler.tick());
}

TEST_CASE("Scheduler active cancellation waits for its outcome and disables retries",
          "[app][scheduler][cancel]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  std::vector<ProtocolEvent> results;
  h.scheduler.setProtocolEventSink([&](ProtocolEvent&& event) {
    results.push_back(std::move(event));
  });
  const auto sid = h.startItem();
  REQUIRE(h.scheduler.cancel(sid));
  REQUIRE(h.scheduler.size() == 1);  // No mid-telegram reset or slot release.
  SECTION("Loss is not retried or reported as exhausted arbitration") {
    ProtocolEvent lost{};
    lost.type = ProtocolEvent::Type::lost;
    lost.session_id = sid;
    REQUIRE(h.settle(lost));
  }
  SECTION("An already-transmitting telegram can still succeed") {
    REQUIRE(h.settle(h.telegramFor(sid)));
  }
  REQUIRE(h.scheduler.size() == 0);
  REQUIRE(results.empty());
  REQUIRE_FALSE(h.scheduler.cancel(sid));
}

TEST_CASE("Scheduler replaces a queued session without needing another free slot",
          "[app][scheduler][replace]") {
  BreakerHarness h;
  const auto message = ebus::toVector("feb5050327002d");
  std::vector<uint32_t> sessions;
  for (size_t i = 0; i < h.scheduler.capacity(); ++i) {
    const auto sid = h.scheduler.enqueue(1, message);
    REQUIRE(sid != 0);
    sessions.push_back(sid);
  }
  const auto old = sessions[sessions.size() / 2];
  SECTION("Full queue permits replacement of an existing queued item") {
    const auto replacement = h.scheduler.enqueueReplacing(1, message, old);
    REQUIRE(replacement != 0);
    REQUIRE(replacement != old);
    REQUIRE(h.scheduler.size() == h.scheduler.capacity());
    REQUIRE_FALSE(h.scheduler.cancel(old));
    REQUIRE(h.scheduler.cancel(replacement));
    for (auto sid : sessions) {
      if (sid != old) REQUIRE(h.scheduler.cancel(sid));
    }
  }
  SECTION("Unknown replacement ID leaves every queued session intact") {
    REQUIRE(h.scheduler.enqueueReplacing(1, message, 0xffffffffu) == 0);
    REQUIRE(h.scheduler.size() == h.scheduler.capacity());
    for (auto sid : sessions) REQUIRE(h.scheduler.cancel(sid));
  }
  REQUIRE(h.scheduler.size() == 0);
}

TEST_CASE("Scheduler replaces a queued retry with a new session",
          "[app][scheduler][replace]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  const auto old = h.startItem();
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = old;
  REQUIRE(h.settle(lost));
  const auto replacement = h.scheduler.enqueueReplacing(
      1, ebus::toVector("feb5050327002e"), old);
  REQUIRE(replacement != 0);
  REQUIRE(h.scheduler.size() == 1);
  REQUIRE_FALSE(h.scheduler.cancel(old));
  REQUIRE(h.scheduler.tick());
  REQUIRE(h.settle(h.telegramFor(replacement)));
  REQUIRE(h.scheduler.size() == 0);
}

TEST_CASE("Scheduler accepted active replacement prevents the old retry",
          "[app][scheduler][replace]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  const auto old = h.startItem();
  const auto replacement = h.scheduler.enqueueReplacing(
      1, ebus::toVector("feb5050327002e"), old);
  REQUIRE(replacement != 0);
  REQUIRE(h.scheduler.size() == 2);
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = old;
  REQUIRE(h.settle(lost));
  REQUIRE(h.scheduler.size() == 1);
  REQUIRE(h.scheduler.tick());
  REQUIRE(h.settle(h.telegramFor(replacement)));
  REQUIRE(h.scheduler.size() == 0);
}

TEST_CASE("Scheduler rejected active replacement preserves the old retry budget",
          "[app][scheduler][replace]") {
  BreakerHarness h;
  h.scheduler.setMaxAttempts(3);
  h.scheduler.setBaseBackoff(1);
  const auto old = h.startItem();
  const auto message = ebus::toVector("feb5050327002d");
  std::vector<uint32_t> other_sessions;
  for (size_t i = 1; i < h.scheduler.capacity(); ++i) {
    const auto sid = h.scheduler.enqueue(1, message);
    REQUIRE(sid != 0);
    other_sessions.push_back(sid);
  }
  REQUIRE(h.scheduler.enqueueReplacing(1, message, old) == 0);
  ProtocolEvent lost{};
  lost.type = ProtocolEvent::Type::lost;
  lost.session_id = old;
  REQUIRE(h.settle(lost));
  REQUIRE(h.scheduler.size() == h.scheduler.capacity());
  for (auto sid : other_sessions) REQUIRE(h.scheduler.cancel(sid));
  REQUIRE(h.scheduler.size() == 1);
  std::this_thread::sleep_until(h.scheduler.nextDueTime());
  REQUIRE(h.scheduler.tick());
  REQUIRE(h.settle(h.telegramFor(old)));
  REQUIRE(h.scheduler.size() == 0);
}
