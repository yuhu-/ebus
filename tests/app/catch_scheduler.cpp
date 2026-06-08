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
#include "test_utils.hpp"

using namespace ebus::detail;

TEST_CASE("Scheduler: Simulation", "[app][scheduler]") {
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

  platform::Queue<ebus::OrchestrationEvent> reactor_queue_{32};

  BusSimulator simulator(bus);

  // BC Success: Broadcast to fe. No slave response.
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("feb5050327002d")),
       ebus::Sequence(), 0, 0});  // 0 is infinite

  // MS Success: Master to 52. Slave responds with payload 01 3f (plus ACK and
  // CRC).
  ebus::Sequence slavePart = ebus::frameSlave(ebus::toVector("013f"));
  ebus::Sequence fullSlaveResponse;
  fullSlaveResponse.pushBack(ebus::Symbols::ack, false);
  fullSlaveResponse.append(slavePart);
  fullSlaveResponse.pushBack(ebus::Symbols::syn,
                             false);  // Master sends SYN after ACK

  // Simulator needs to know about the full expected sequence including the
  // final SYN
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("52b509030d4600")),
       fullSlaveResponse, 1, 4});

  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("52b509030d4600")),
       fullSlaveResponse, 1, 4});

  // Retry Success: Master to fe. Simulate NAK (ff) twice, then success on 3rd
  // try.
  auto retry_trigger = ebus::frameMaster(source, ebus::toVector("fe070400"));
  simulator.addMockReaction(
      {retry_trigger, ebus::Sequence({ebus::Symbols::nak}), 2, 0});

  ebus::Sequence retry_success_action;
  retry_success_action.pushBack(ebus::Symbols::ack, false);
  retry_success_action.append(ebus::frameSlave(ebus::toVector("013f")));
  retry_success_action.pushBack(ebus::Symbols::syn, false);

  simulator.addMockReaction(
      {retry_trigger, std::move(retry_success_action), 1, 0});

  Scheduler scheduler(&handler);
  scheduler.setEventSink([&](ebus::OrchestrationEvent&& ev) {
    reactor_queue_.tryPush(std::move(ev));
  });
  scheduler.attachHandlerCallbacks();
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  bus.start();

  // Bridge Physical Bus Events -> Unified Reactor Queue
  bus.addBusEventListener([&](const BusEvent& bus_ev) {
    ebus::OrchestrationEvent oev;
    oev.type = ebus::OrchestrationEventType::bus_byte;
    oev.data.byte_data.val = bus_ev.byte;
    oev.data.byte_data.bus_request = bus_ev.bus_request;
    oev.data.byte_data.start_bit = bus_ev.start_bit;
    oev.data.byte_data.timestamp = bus_ev.timestamp;
    reactor_queue_.tryPush(std::move(oev));
  });

  std::atomic<uint32_t> last_success_session{0};
  std::atomic<uint32_t> last_error_session{0};

  scheduler.setProtocolCallback([&](const ebus::ProtocolInfo& info) {
    if (info.is_error) {
      last_error_session.store(info.session_id);
    } else if (info.message_type == ebus::MessageType::active) {
      last_success_session.store(info.session_id);
    }
  });

  // Simulate the Controller's reactor loop for the duration of the tests
  auto test_start_time = ebus::Clock::now();
  const auto test_timeout =
      std::chrono::seconds(10);  // Increased overall test timeout

  bool all_tests_passed = true;
  std::vector<std::string> payloads = {"feb5050327002d", "52b509030d4600",
                                       "fe070400"};

  for (const auto& payload : payloads) {
    last_success_session.store(0);
    last_error_session.store(0);

    uint32_t session_id = scheduler.enqueue(1, ebus::toVector(payload));
    REQUIRE(session_id > 0);

    auto current_test_start = ebus::Clock::now();
    while (last_success_session.load() != session_id &&
           last_error_session.load() != session_id &&
           (ebus::Clock::now() - current_test_start) <
               std::chrono::seconds(3)) {  // Timeout per message
      scheduler.tick();

      ebus::OrchestrationEvent ev;
      auto next_sched_due = scheduler.nextDueTime();
      auto now = ebus::Clock::now();
      uint32_t timeout_ms = 0;
      if (next_sched_due == ebus::Clock::time_point::max()) {
        timeout_ms = 10;  // Default small timeout if nothing is scheduled
      } else if (next_sched_due > now) {
        timeout_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                next_sched_due - now)
                .count());
        if (timeout_ms == 0)
          timeout_ms = 1;  // Ensure at least 1ms if due very soon
      } else {
        timeout_ms = 0;  // Already due or in the past, process immediately
      }

      if (reactor_queue_.pop(ev, timeout_ms)) {
        switch (ev.type) {
          case ebus::OrchestrationEventType::bus_byte: {
            BusEvent bus_ev{
                ev.data.byte_data.val, ev.data.byte_data.bus_request,
                ev.data.byte_data.start_bit, ev.data.byte_data.timestamp};
            busHandler.processEvent(bus_ev);
            break;
          }
          case ebus::OrchestrationEventType::protocol_result: {
            scheduler.injectProtocolEvent(ev.data.protocol_data);
            break;
          }
          default:
            break;
        }
      }
    }
    if (last_success_session.load() != session_id) {
      all_tests_passed = false;
      WARN("Test failed for payload: " << payload);
    }
  }

  REQUIRE(all_tests_passed);

  bus.stop();
}
