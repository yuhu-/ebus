/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <ebus/controller.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "utils/common.hpp"

/**
 * Legacy-style test helper.
 */
void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

int main() {
  std::cout << "--- Test: Controller Lifecycle and API ---" << std::endl;

  // 1. Configuration
  ebus::EbusConfig config;
  config.runtime.address = 0x31;
  config.bus.simulate = true;
  config.runtime.enable_syn =
      true;  // Use internal SYN generator for simulation

  ebus::Controller controller(config);
  run_test("Controller is configured", controller.isConfigured());

  // 2. Setup Central Dispatcher Callback
  bool telegramSeen = false;
  controller.setTelegramCallback(
      [&](const ebus::MessageType&, const ebus::TelegramType&,
          const std::vector<uint8_t>& master, const std::vector<uint8_t>&) {
        std::cout << "  Dispatcher -> Observed Telegram: "
                  << ebus::toString(master) << std::endl;
        telegramSeen = true;
      });

  // 3. Start the service
  controller.start();
  run_test("Controller is running", controller.isRunning());

  // 4. Test Active Messaging (Enqueue)
  // Broadcast Telegram: [fe] [b5] [05] [04] [27] [00] [2d] [00]
  std::vector<uint8_t> msg = {0xfe, 0xb5, 0x05, 0x04, 0x27, 0x00, 0x2d, 0x00};
  bool resultCallbackFired = false;

  std::cout << "  Enqueuing broadcast message..." << std::endl;
  controller.enqueue(1, msg,
                     [&](bool success, const std::vector<uint8_t>&,
                         const std::vector<uint8_t>&) {
                       run_test("Scheduler result callback fired", success);
                       resultCallbackFired = true;
                     });

  // Wait for the background threads to process.
  // The simulation needs time for: SYN generation -> Arbitration ->
  // Transmission -> Echo Dispatch.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  run_test("Result callback was called", resultCallbackFired);
  run_test("Central dispatcher pushed the telegram", telegramSeen);

  // 5. Verify Diagnostics (Metrics)
  auto metrics = controller.getMetrics();
  run_test("Metrics collection is active", !metrics.empty());
  if (metrics.count("handler.counter.messagesTotal")) {
    std::cout << "  Total messages processed: "
              << metrics["handler.counter.messagesTotal"].last << std::endl;
    run_test("Message count is at least 1",
             metrics["handler.counter.messagesTotal"].last >= 1);
  }

  // 6. Shutdown
  std::cout << "  Stopping controller..." << std::endl;
  controller.stop();
  run_test("Controller is no longer running", !controller.isRunning());

  std::cout << "\nAll Controller tests passed!" << std::endl;

  return EXIT_SUCCESS;
}