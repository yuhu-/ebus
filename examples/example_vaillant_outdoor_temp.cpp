/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Example: Decoding Vaillant Outdoor Temperature Broadcasts
 *
 * This example shows how to configure a Controller to listen for
 * specific manufacturer broadcast messages and decode the payload.
 */

#include <ebus.hpp>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
  // --- 1. Setup Simulation ---
  // We use simulation mode to demonstrate the logic without hardware.
  ebus::EbusConfig config;
  config.bus.simulate = true;
  config.runtime.address = 0x31;          // Our controller address
  config.runtime.bus.syn.enabled = true;  // MUST be enabled for simulation
  config.runtime.lock_counter = 0;        // Send immediately on first SYN

  ebus::Controller controller(config);

  // --- 2. Define Decoder Logic ---
  // The Vaillant outdoor temperature is typically broadcasted (FE)
  // via Service 0xB5 0x16 with a payload identifying the sensor.
  controller.setTelegramCallback([](const ebus::TelegramInfo& info) {
    // master_view layout: [Source, Destination, PB, SB, NN, Data...]
    // We check for: Destination=FE, PB=B5, SB=16, NN=03, DB1=01
    if (info.telegram_type == ebus::TelegramType::broadcast) {
      if (ebus::matches(info.master_view, {0xfe, 0xb5, 0x16, 0x03, 0x01}, 1)) {
        // The temperature is stored in DB2 and DB3 using DATA2B type (1/256
        // precision). In master_view, these are at indices 6 and 7.
        auto temp_data = ebus::range(info.master_view, 6, 2);
        auto val = ebus::decode(ebus::DataType::data2b, temp_data);

        if (val) {
          std::cout << "[Monitor] Detected Vaillant temperature broadcast: "
                    << ebus::toString(*val) << " °C" << std::endl;
        }
      }
    }
  });

  std::cout << "Starting Vaillant temperature monitor example..." << std::endl;
  controller.start();

  // --- 3. Simulate Traffic ---
  // Create a raw message representing a temperature of 12.5°C
  // DATA2B: 12.5 * 256 = 3200 = 0x0C80 -> bytes: 0x80, 0x0C
  // Telegram components: ZZ=FE, PB=B5, SB=16, NN=03, Data=[01, 80, 0C]
  std::vector<uint8_t> msg = {0xfe, 0xb5, 0x16, 0x03, 0x01, 0x80, 0x0c};

  // Enqueue with priority 10 (overrides background scan traffic priority 5)
  controller.enqueue(10, msg);

  // Wait to observe the decoded output
  std::this_thread::sleep_for(3s);  // Increased wait time

  std::cout << "Stopping example." << std::endl;
  controller.stop();

  return 0;
}
