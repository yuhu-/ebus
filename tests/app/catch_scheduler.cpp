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
  BusHandler busHandler(&request, &handler, bus.getQueue());

  const uint8_t source = 0x01;
  handler.setSourceAddress(source);

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

  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("52b509030d4600")),
       fullSlaveResponse, 1, 5});

  // Retry Success: Master to fe. Simulate NAK (ff) twice, then success on 3rd
  // try.
  auto retry_trigger = ebus::frameMaster(source, ebus::toVector("fe070400"));
  simulator.addMockReaction(
      {retry_trigger, ebus::Sequence({ebus::Symbols::nak}), 2, 0});

  ebus::Sequence retry_success_action;
  retry_success_action.pushBack(ebus::Symbols::ack, false);
  retry_success_action.append(ebus::frameSlave(ebus::toVector("013f")));

  simulator.addMockReaction(
      {retry_trigger, std::move(retry_success_action), 1, 0});

  Scheduler scheduler(&handler);
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  scheduler.setTelegramCallback([](const ebus::TelegramInfo& info) {
    std::cerr << "[Handler Telegram] session_id: " << info.session_id
              << ", poll_id: " << info.poll_id
              << ", retry_count: " << info.retry_count
              << ", message_type: " << ebus::toString(info.message_type)
              << ", telegram_type: " << ebus::toString(info.telegram_type)
              << ", h_state: " << ebus::toString(info.handler_state)
              << ", r_state: " << ebus::toString(info.request_state)
              << ", master: " << ebus::toString(info.master_view)
              << ", slave: " << ebus::toString(info.slave_view) << std::endl;
  });

  scheduler.setErrorCallback([](const ebus::ErrorInfo& info) {
    std::cerr << "[Handler Error] protocol_error: "
              << ebus::toString(info.protocol_error)
              << ", result: " << ebus::toString(info.result)
              << ", seq_state: " << ebus::toString(info.sequence_state)
              << ", h_state: " << ebus::toString(info.handler_state)
              << ", r_state: " << ebus::toString(info.request_state)
              << ", master: " << ebus::toString(info.master_view)
              << ", slave: " << ebus::toString(info.slave_view) << std::endl;
  });

  bus.start();
  busHandler.start();
  scheduler.start();

  auto run_test = [&](const std::string& payload) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    scheduler.enqueue(
        1, ebus::toVector(payload), [promise](const ebus::ResultInfo& info) {
          if (!info.success) {
            std::cerr << "[Scheduler Result] FAIL: result="
                      << ebus::toString(info.result)
                      << ", seq_state=" << ebus::toString(info.sequence_state)
                      << ", master=" << ebus::toString(info.master_view)
                      << ", slave=" << ebus::toString(info.slave_view)
                      << std::endl;
          }
          promise->set_value(info.success);
        });
    return future.wait_for(std::chrono::seconds(2)) ==
               std::future_status::ready &&
           future.get();
  };

  REQUIRE(run_test("feb5050327002d"));
  REQUIRE(run_test("52b509030d4600"));
  REQUIRE(run_test("fe070400"));

  scheduler.stop();
  busHandler.stop();
  bus.stop();
}

TEST_CASE("Scheduler: Priority Preemption", "[app][scheduler][priority]") {
  Request request;
  ebus::BusConfig config;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x01;
  runtime.bus.syn_gen = true;
  runtime.lock_counter = 0;  // Send immediately

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);
  BusHandler busHandler(&request, &handler, bus.getQueue());

  BusSimulator simulator(bus);
  const uint8_t source = 0x01;

  // Setup responses for both test messages
  // High priority trigger
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("feb50500")), ebus::Sequence(),
       1, 0});
  // Low priority trigger
  simulator.addMockReaction(
      {ebus::frameMaster(source, ebus::toVector("fe070400")), ebus::Sequence(),
       1, 0});

  Scheduler scheduler(&handler);
  scheduler.start();
  bus.start();
  busHandler.start();

  std::vector<uint8_t> execution_order;
  std::mutex order_mutex;

  // We use a future timestamp to ensure both items are in the queue
  // before the scheduler picks the next one.
  auto start_time = ebus::Clock::now() + std::chrono::milliseconds(100);

  // 1. Enqueue Low Priority (5) first
  scheduler.enqueueAt(5, ebus::toVector("fe070400"), start_time,
                      [&](const ebus::ResultInfo&) {
                        std::lock_guard<std::mutex> lock(order_mutex);
                        execution_order.push_back(5);
                      });

  // 2. Enqueue High Priority (255) second
  scheduler.enqueueAt(255, ebus::toVector("feb50500"), start_time,
                      [&](const ebus::ResultInfo&) {
                        std::lock_guard<std::mutex> lock(order_mutex);
                        execution_order.push_back(255);
                      });

  // Wait for both tasks to complete
  bool completed = waitCondition(
      [&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        return execution_order.size() == 2;
      },
      3000);  // Increased timeout for robustness

  scheduler.stop();  // Stop scheduler before checking local variables
  busHandler.stop();
  bus.stop();
  simulator.clear();  // Clear simulator workers

  REQUIRE(completed);  // Check completion after stopping threads

  // Verify that 255 preempted 5
  REQUIRE(execution_order[0] == 255);
  REQUIRE(execution_order[1] == 5);
}
