/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstddef>
#include <ebus.hpp>
#include <ebus/data_types.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/telegram.hpp"

using namespace std::chrono_literals;

int main() {
#if EBUS_SIMULATION
  std::cout << "Playground: Running Integration Test via VirtualBus..."
            << std::endl;

  ebus::EbusConfig config;
  config.runtime.address = 0x31;
  config.runtime.bus.syn_gen = true;

  ebus::Controller controller(config);
  ebus::VirtualBus& vbus = controller.getVirtualBus();

  // Mock a Vaillant sensor response (Service 07 04 - Identification)
  // Trigger when OUR controller (config.runtime.address) sends a request to
  // slave 0x15.
  vbus.addSlaveReaction(config.runtime.address, "15070400",
                        "0ab54d4f434b0001020304");

  controller.setTelegramCallback([](const ebus::TelegramInfo& info) {
    std::cout << "[Telegram] " << ebus::toString(info.telegram_type)
              << " Master: " << ebus::toString(info.master_view)
              << " Slave: " << ebus::toString(info.slave_view) << std::endl;
  });

  controller.start();

  // Test Case: Identification Request
  controller.enqueue(
      10, ebus::toVector("15070400"), [](const ebus::ResultInfo& res) {
        if (res.success) {
          std::cout
              << "[Result] Integration Test SUCCESS: Received MOCK response"
              << std::endl;
        } else {
          std::cout << "[Result] Integration Test FAILED: "
                    << ebus::toString(res.result) << std::endl;
        }
      });

  std::this_thread::sleep_for(2s);
  controller.stop();
#else
  std::cerr << "Playground requires EBUS_SIMULATION=ON for modern integration "
               "testing."
            << std::endl;
#endif
  return 0;
}
