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
#include "test_utils.hpp"
#include <thread>
#include <vector>

TEST_CASE("Controller: Lifecycle and API", "[app][controller]") {
  // Configuration
  ebus::EbusConfig config;
  config.runtime.address = 0x31;
  config.bus.simulate = true;
  config.runtime.bus.syn.enabled = true;

  ebus::Controller controller(config);
  REQUIRE(controller.isConfigured());

  // Central dispatcher callback
  std::atomic<bool> telegramSeen{false};
  controller.setTelegramCallback(
      [&](const ebus::TelegramInfo& info) { telegramSeen = true; });

  // Start service
  controller.start();
  REQUIRE(controller.isRunning());

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
  REQUIRE(metrics.handler.messages_total == 1);

  // Shutdown
  controller.stop();
  REQUIRE(!controller.isRunning());
}
