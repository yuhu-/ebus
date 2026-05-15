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
#include "test_utils.hpp"
#include "utils/bus_simulator.hpp"

// using namespace ebus;
using namespace ebus::detail;

TEST_CASE("Scheduler: Simulation", "[app][scheduler]") {
  Request request;
  ebus::BusConfig config;
  config.device = "/dev/null";

  ebus::RuntimeConfig runtime;
  runtime.address = 0x33;
  runtime.bus.syn_gen = true;

  BusMonitor monitor;
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(ebus::RuntimeConfig{}.address, &bus, &request, &monitor);
  BusHandler busHandler(&request, &handler, bus.getQueue());

  const uint8_t source = 0x33;
  handler.setSourceAddress(source);

  BusSimulator simulator(bus);

  // BC Success: Broadcast to fe. No slave response.
  simulator.addResponse(
      {ebus::toVector(frameMasterHex(source, "feb5050327002d")), {}, 0});

  // MS Success: Master to 52. Slave responds with payload 01 3f (plus ACK and
  // CRC).
  simulator.addResponse(source, "52b509030d4600", "013f");

  // Retry Success: Master to fe. Simulate NAK (ff) twice, then success on 3rd
  // try.
  auto retry_trigger = ebus::toVector(frameMasterHex(source, "fe070400"));
  simulator.addResponse({retry_trigger, {ebus::Symbols::nak}, 2});
  simulator.addResponse(
      {retry_trigger, ebus::toVector(frameSlaveHex("013f")), 1});

  Scheduler scheduler(&handler);
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(50);

  bus.start();
  busHandler.start();
  scheduler.start();

  auto run_test = [&](const std::string& payload) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    scheduler.enqueue(1, ebus::toVector(payload),
                      [promise](const ebus::ResultInfo& info) {
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
  config.device = "/dev/null";

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
  simulator.addResponse(
      {ebus::toVector(frameMasterHex(source, "feb50500")), {}, 0});
  // Low priority trigger
  simulator.addResponse(
      {ebus::toVector(frameMasterHex(source, "fe070400")), {}, 0});

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
