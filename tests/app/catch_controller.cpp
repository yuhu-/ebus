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

TEST_CASE("Controller: Lifecycle and API", "[app][controller]") {
  // Configuration
  ebus::EbusConfig config;
  config.runtime.address = 0x31;
  config.bus.simulate = true;
  config.runtime.enable_syn = true;

  ebus::Controller controller(config);
  REQUIRE(controller.isConfigured());

  // Central dispatcher callback
  std::atomic<bool> telegramSeen{false};
  controller.setTelegramCallback(
      [&](ebus::MessageType message_type, ebus::TelegramType telegram_type,
          ebus::ByteView master_view,
          ebus::ByteView slave_view) { telegramSeen = true; });

  // Start service
  controller.start();
  REQUIRE(controller.isRunning());

  // Active messaging (enqueue)
  std::vector<uint8_t> msg = {0xfe, 0xb5, 0x05, 0x04, 0x27, 0x00, 0x2d, 0x00};
  std::atomic<bool> resultCallbackFired{false};

  controller.enqueue(
      1, msg,
      [&](bool success, ebus::ByteView master_view, ebus::ByteView slave_view) {
        REQUIRE(success);
        resultCallbackFired = true;
      });

  // Allow background processing (SYN/arbitration/dispatch)
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  REQUIRE(resultCallbackFired.load());
  REQUIRE(telegramSeen.load());

  // Metrics
  auto metrics = controller.getMetrics();
  REQUIRE(!metrics.empty());
  if (metrics.count("handler.counter.messages_total")) {
    REQUIRE(metrics["handler.counter.messages_total"].last >= 1);
  }

  // Shutdown
  controller.stop();
  REQUIRE(!controller.isRunning());
}
