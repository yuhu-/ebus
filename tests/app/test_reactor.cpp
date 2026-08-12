/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/callbacks.hpp>
#include <ebus/types.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <thread>
#include <vector>

#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "app/poll_manager.hpp"
#include "app/reactor.hpp"
#include "app/scheduler.hpp"
#include "core/bus_handler.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/system.hpp"
#include "test_helpers.hpp"

using namespace ebus;
using namespace ebus::detail;

// --- Unit tests with nullptr dependencies (no worker thread) ---

TEST_CASE("Reactor: Signal Queue Push, Size, and Max Tracking",
          "[app][reactor][queue]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  REQUIRE(reactor.signalQueueSize() == 0);
  REQUIRE(reactor.maxSignalQueueSize() == 0);

  ReactorSignal sig;
  sig.type = ReactorSignal::Type::user_request;
  REQUIRE(reactor.pushSignal(std::move(sig)));
  REQUIRE(reactor.signalQueueSize() == 1);
  REQUIRE(reactor.maxSignalQueueSize() == 1);

  ReactorSignal sig2;
  sig2.type = ReactorSignal::Type::timer_wakeup;
  REQUIRE(reactor.pushSignal(std::move(sig2)));
  REQUIRE(reactor.signalQueueSize() == 2);
  REQUIRE(reactor.maxSignalQueueSize() == 2);
}

TEST_CASE("Reactor: Signal Queue Exhaustion", "[app][reactor][queue]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  ReactorSignal sig;
  sig.type = ReactorSignal::Type::bus_byte;

  const size_t capacity = ReactorLimits::signal_queue_size;
  for (size_t i = 0; i < capacity; ++i) {
    REQUIRE(reactor.pushSignal(ReactorSignal{sig.type}));
  }
  REQUIRE(reactor.signalQueueSize() == capacity);

  REQUIRE(reactor.pushSignal(std::move(sig)));
  REQUIRE(reactor.signalQueueSize() == capacity);
}

TEST_CASE("Reactor: Protocol Event Queue Push, Size, and Max Tracking",
          "[app][reactor][queue]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  REQUIRE(reactor.protocolQueueSize() == 0);
  REQUIRE(reactor.maxProtocolQueueSize() == 0);

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::won;
  ev.session_id = 1;

  REQUIRE(reactor.pushProtocolEvent(std::move(ev)));
  REQUIRE(reactor.protocolQueueSize() == 1);
  REQUIRE(reactor.maxProtocolQueueSize() == 1);

  ProtocolEvent ev2{};
  ev2.type = ProtocolEvent::Type::lost;
  ev2.session_id = 2;
  REQUIRE(reactor.pushProtocolEvent(std::move(ev2)));
  REQUIRE(reactor.protocolQueueSize() == 2);
  REQUIRE(reactor.maxProtocolQueueSize() == 2);
}

TEST_CASE("Reactor: Protocol Event Queue Drain Produces Callback Signal",
          "[app][reactor][queue]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::won;
  ev.session_id = 1;

  const size_t cap = ReactorLimits::protocol_queue_size;
  for (size_t i = 0; i < cap; ++i) {
    REQUIRE(reactor.pushProtocolEvent(std::move(ev)));
  }
  REQUIRE(reactor.protocolQueueSize() == cap);

  REQUIRE(reactor.pushProtocolEvent(std::move(ev)));
  REQUIRE(reactor.signalQueueSize() == cap + 1);
}

TEST_CASE("Reactor: Trace Buffer from Bus Events", "[app][reactor][trace]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  std::atomic<int> trace_count{0};
  reactor.setTraceCallback([&](const BusEventInfo& info) { trace_count++; });

  for (int i = 0; i < 5; ++i) {
    BusEventInfo info;
    info.byte = static_cast<uint8_t>(0x10 + i);
    reactor.onBusEventInfo(info);
  }

  REQUIRE(reactor.signalQueueSize() == 5);
  REQUIRE(reactor.protocolQueueSize() == 0);

  int visited = 0;
  reactor.fetchTraceHistory([&](const BusEventInfo& info) { visited++; });
  REQUIRE(visited == 5);
  REQUIRE(trace_count.load() == 0);
}

TEST_CASE("Reactor: Trace Buffer JSON Visitor", "[app][reactor][trace]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  for (int i = 0; i < 3; ++i) {
    BusEventInfo info;
    info.byte = static_cast<uint8_t>(i);
    reactor.onBusEventInfo(info);
  }

  std::string json_output;
  reactor.fetchTraceHistory(
      [&](std::string_view chunk) { json_output += chunk; }, true);
  REQUIRE_FALSE(json_output.empty());
}

TEST_CASE("Reactor: Error Buffer Initial State and Capacity",
          "[app][reactor][diagnostics]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  REQUIRE(reactor.getErrorLogCapacity() ==
          DiagnosticsLimits::error_history_size);

  int error_count = 0;
  reactor.fetchErrors([&](const ErrorEntry& entry) { error_count++; });
  REQUIRE(error_count == 0);

  reactor.clearErrors();
}

TEST_CASE("Reactor: Error Buffer JSON Visitor", "[app][reactor][diagnostics]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  std::string json_output;
  reactor.fetchErrors([&](std::string_view chunk) { json_output += chunk; },
                      true);
  REQUIRE_FALSE(json_output.empty());
}

TEST_CASE("Reactor: Callback Registration and Worker Null Before Start",
          "[app][reactor][lifecycle]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);

  REQUIRE(reactor.worker() == nullptr);

  std::atomic<bool> proto_called{false};
  std::atomic<bool> trace_called{false};

  reactor.setProtocolCallback(
      [&](const ProtocolInfo& info) { proto_called.store(true); });
  reactor.setTraceCallback(
      [&](const BusEventInfo& info) { trace_called.store(true); });
  reactor.setLogLevel(LogLevel::debug);

  REQUIRE_FALSE(proto_called.load());
  REQUIRE_FALSE(trace_called.load());
}

TEST_CASE("Reactor: Stop Without Start Does Not Crash",
          "[app][reactor][lifecycle]") {
  Reactor reactor(0x10, false, nullptr, nullptr, nullptr, nullptr, nullptr);
  reactor.stop();
  REQUIRE(reactor.worker() == nullptr);
}

// --- Integration tests with full bus stack (worker thread) ---

struct ReactorTestEnv {
  BusMonitor monitor;
  Request request;
  platform::Bus bus;
  Handler handler;
  BusHandler bus_handler;
  Scheduler scheduler;
  DeviceManager device_manager;
  DeviceScanner device_scanner;
  PollManager poll_manager;
  Reactor reactor;

  ReactorTestEnv(uint8_t addr, bool system_response)
      : request(&monitor),
        bus(BusConfig{}, makeRuntime(addr), &request, &monitor),
        handler(addr, &bus, &request, &monitor),
        bus_handler(&request, &handler),
        scheduler(&handler),
        device_manager(&monitor),
        device_scanner(addr, &device_manager),
        reactor(addr, system_response, &scheduler, &poll_manager,
                &device_scanner, &device_manager, &monitor) {
    scheduler.attachHandlerCallbacks();
    scheduler.setProtocolEventSink([this](ProtocolEvent&& ev) {
      reactor.pushProtocolEvent(std::move(ev));
    });
    bus_handler.setReactorBusEventInfoCallback(
        Delegate<void(const BusEventInfo&)>::bind<Reactor,
                                                  &Reactor::onBusEventInfo>(
            &reactor));
  }

  static RuntimeConfig makeRuntime(uint8_t addr) {
    RuntimeConfig runtime;
    runtime.address = addr;
    runtime.bus.syn_gen = true;
    return runtime;
  }
};

TEST_CASE("Reactor: Full Lifecycle Start Stop", "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  REQUIRE(env.reactor.worker() == nullptr);

  env.bus.start();
  env.reactor.start();

  REQUIRE(env.reactor.worker() != nullptr);

  env.reactor.stop();
  env.bus.stop();

  REQUIRE(env.reactor.worker() == nullptr);
}

TEST_CASE("Reactor: Shutdown Signal Stops Worker Thread",
          "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  env.bus.start();
  env.reactor.start();

  ReactorSignal sig;
  sig.type = ReactorSignal::Type::shutdown;
  env.reactor.pushSignal(std::move(sig));

  env.reactor.stop();

  auto worker_null = [&] { return env.reactor.worker() == nullptr; };
  REQUIRE(waitCondition(worker_null, 2000));

  env.bus.stop();
}

TEST_CASE("Reactor: Trace Callback Dispatch on Bus Event",
          "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  std::atomic<int> trace_count{0};
  env.reactor.setTraceCallback(
      [&](const BusEventInfo& info) { trace_count++; });

  env.bus.start();
  env.reactor.start();

  BusEventInfo info;
  info.byte = 0xfe;
  info.handler_state = HandlerState::passive_receive_master;
  env.reactor.onBusEventInfo(info);

  auto trace_seen = [&] { return trace_count.load() >= 1; };
  REQUIRE(waitCondition(trace_seen, 2000));

  env.reactor.stop();
  env.bus.stop();
}

TEST_CASE("Reactor: Protocol Callback Dispatch on Telegram Event",
          "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  std::atomic<uint32_t> received_session{0};
  env.reactor.setProtocolCallback([&](const ProtocolInfo& info) {
    if (!info.is_error) {
      received_session.store(info.session_id);
    }
  });

  env.bus.start();
  env.reactor.start();

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::telegram;
  ev.session_id = 42;
  ev.message_type = MessageType::active;
  ev.telegram_type = TelegramType::broadcast;
  ev.master.assign(toVector("feb50503").data(), 4);

  env.reactor.pushProtocolEvent(std::move(ev));

  auto session_ok = [&] { return received_session.load() == 42; };
  REQUIRE(waitCondition(session_ok, 2000));

  env.reactor.stop();
  env.bus.stop();
}

TEST_CASE("Reactor: Protocol Callback Dispatch on Error Event",
          "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  std::atomic<uint32_t> error_count{0};
  std::atomic<ProtocolError> last_error{};
  env.reactor.setProtocolCallback([&](const ProtocolInfo& info) {
    if (info.is_error) {
      error_count++;
      last_error.store(info.protocol_error);
    }
  });

  env.bus.start();
  env.reactor.start();

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::error;
  ev.session_id = 99;
  ev.retry_count = 3;
  ev.handler_state = HandlerState::passive_receive_master;
  ev.request_state = RequestState::observe;
  ev.protocol_error = ProtocolError::invalid_message;
  ev.result = RequestResult::first_error;
  ev.sequence_state = SequenceState::seq_empty;
  ev.level = LogLevel::error;
  ev.master.assign(toVector("feb50503").data(), 4);

  env.reactor.pushProtocolEvent(std::move(ev));

  auto error_seen = [&] { return error_count.load() >= 1; };
  REQUIRE(waitCondition(error_seen, 2000));
  REQUIRE(last_error.load() == ProtocolError::invalid_message);

  int error_entries = 0;
  env.reactor.fetchErrors([&](const ErrorEntry& entry) { error_entries++; });
  REQUIRE(error_entries >= 1);

  env.reactor.stop();
  env.bus.stop();
}

TEST_CASE("Reactor: Error Buffer Cleared After clearErrors",
          "[app][reactor][integration]") {
  ReactorTestEnv env(0x10, false);

  env.bus.start();
  env.reactor.start();

  ProtocolEvent ev{};
  ev.type = ProtocolEvent::Type::error;
  ev.session_id = 1;
  ev.protocol_error = ProtocolError::invalid_message;
  ev.result = RequestResult::first_error;
  ev.sequence_state = SequenceState::seq_empty;
  ev.level = LogLevel::error;
  ev.handler_state = HandlerState::passive_receive_master;
  ev.request_state = RequestState::observe;

  env.reactor.pushProtocolEvent(std::move(ev));

  auto has_errors = [&] {
    int count = 0;
    env.reactor.fetchErrors([&](const ErrorEntry&) { count++; });
    return count >= 1;
  };
  REQUIRE(waitCondition(has_errors, 2000));

  env.reactor.clearErrors();

  int count_after_clear = 0;
  env.reactor.fetchErrors([&](const ErrorEntry&) { count_after_clear++; });
  REQUIRE(count_after_clear == 0);

  env.reactor.stop();
  env.bus.stop();
}
