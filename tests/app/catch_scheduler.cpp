/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/data_types.hpp>
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

TEST_CASE("Scheduler: Simulation", "[app][scheduler]") {
  ebus::Request request;
  ebus::BusConfig config;
  config.device = "/dev/null";
  config.simulate = true;

  ebus::RuntimeConfig runtime;
  runtime.address = 0x33;
  runtime.bus.syn.enabled = true;

  ebus::BusMonitor monitor;
  ebus::Bus bus(config, runtime, &request, &monitor);
  ebus::Handler handler(ebus::defaults::address, &bus, &request, &monitor);
  ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

  const uint8_t source = 0x33;
  handler.setSourceAddress(source);

  ebus::BusSimulator simulator(bus);

  // BC Success: Broadcast to fe. No slave response.
  simulator.addResponse(
      {ebus::toVector(frameMasterHex(source, "feb5050327002d")), {}, 0});

  // MS Success: Master to 52. Slave responds with payload 01 3f (plus ACK and
  // CRC).
  simulator.addMasterSlaveResponse(source, "52b509030d4600", "013f", 5);

  // Retry Success: Master to fe. Simulate NAK (ff) twice, then success on 3rd
  // try.
  auto retry_trigger = ebus::toVector(frameMasterHex(source, "fe070400"));
  simulator.addResponse({retry_trigger, {ebus::Symbols::nak}, 5, 2});
  simulator.addResponse(
      {retry_trigger, ebus::toVector(frameSlaveHex("013f")), 5, 1});

  ebus::Scheduler scheduler(&handler);
  scheduler.setMaxSendAttempts(3);
  scheduler.setBaseBackoff(std::chrono::milliseconds(50));

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
