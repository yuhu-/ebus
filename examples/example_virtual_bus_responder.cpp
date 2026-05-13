/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Example: Virtual Bus eBUS Simulation with Automated Responses
 *
 * This example demonstrates how to use the ebus::VirtualBus API to
 * set up automated responses for simulated eBUS traffic and inject
 * messages from external simulated participants.
 */

#include <chrono>
#include <ebus.hpp>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
  // --- 1. Configuration for the Main Controller (Master 0x01) ---
  // This controller will run in simulation mode and interact with its own
  // VirtualBus instance, which will simulate other devices.
  ebus::EbusConfig config;
  config.runtime.address = 0x01;  // Our controller address
  config.bus.simulate = true;     // Enable simulation mode
  config.runtime.bus.syn_gen =
      true;  // SYN generation is crucial for simulation
  config.runtime.system_inquiry = false;
  config.runtime.system_response = false;
  config.runtime.scanner.scan_on_startup = false;

  ebus::Controller controller(config);

  // --- 2. Access the VirtualBus API ---
  // The VirtualBus instance allows direct control over the simulated bus.
  ebus::VirtualBus& virtualBus = controller.getVirtualBus();

  // --- 3. Configure an Automated Response (Simulating a Slave) ---
  // We want to simulate a slave device at address 0x15.
  // When our controller (0x01) sends an ID request to 0x15 (15070400),
  // the VirtualBus should automatically respond as if it were 0x15.
  std::cout << "[VirtualBus] Configuring automated response for slave 0x15 ID "
               "request..."
            << std::endl;
  virtualBus.addMasterSlaveResponse(
      0x01,        // Source of the master request (our controller)
      "15070400",  // Master payload: request ID from 0x15
      "0ab54d4f434b0001020304",  // Slave response: mock ID data
      5                          // Delay in ms for the response
  );

  // --- 4. Configure another Automated Response (Simulating a Broadcast
  // Listener) --- Simulate a device that responds to a specific broadcast.
  // Let's say a broadcast "fe070000" (Inquiry of Existence) triggers a simple
  // ACK from a listening device.
  std::cout << "[VirtualBus] Configuring automated response for broadcast "
               "'fe070000'..."
            << std::endl;
  virtualBus.addResponse({
      ebus::toVector("fe070000"),  // Trigger pattern (broadcast message)
      ebus::toVector(
          "00"),  // Response (e.g., a simple ACK from a listening device)
      2,          // Delay in ms
      1           // Repeat once
  });

  // --- 5. Set up a Telegram Callback to Observe Traffic ---
  // This callback will be invoked for all successfully processed telegrams.
  controller.setTelegramCallback([](const ebus::TelegramInfo& info) {
    std::cout << "[Controller] Telegram received: Type="
              << ebus::toString(info.telegram_type)
              << ", Master=" << ebus::toString(info.master_view)
              << ", Slave=" << ebus::toString(info.slave_view) << std::endl;
  });

  // --- 6. Start the Controller ---
  std::cout << "Starting controller in simulation mode..." << std::endl;
  controller.start();
  std::this_thread::sleep_for(100ms);  // Give threads a moment to start

  // --- 7. Trigger the Automated Response from our Controller ---
  // Enqueue a message from our controller (0x01) to the simulated slave (0x15).
  std::cout << "[Controller] Enqueuing ID request to simulated slave 0x15..."
            << std::endl;
  controller.enqueue(
      10, ebus::toVector("15070400"), [](const ebus::ResultInfo& info) {
        if (info.success) {
          std::cout
              << "[Controller] ID request to 0x15 successful. Slave response: "
              << ebus::toString(info.slave_view) << std::endl;
        } else {
          std::cout << "[Controller] ID request to 0x15 failed: "
                    << ebus::toString(info.result) << std::endl;
        }
      });

  // --- 8. Inject an External Message (Simulating another Master) ---
  // Simulate a broadcast message coming from an external master (0x03)
  std::cout << "[VirtualBus] Injecting broadcast from simulated external "
               "master 0x03..."
            << std::endl;
  virtualBus.injectMasterMessage(0x03, ebus::toVector("fe070000"));

  // --- 9. Run for a while to observe interactions ---
  std::cout << "Running simulation for 5 seconds..." << std::endl;
  std::this_thread::sleep_for(5s);

  // --- 10. Clear automated responses ---
  std::cout << "[VirtualBus] Clearing all automated responses." << std::endl;
  virtualBus.clear();

  // --- 11. Attempt to trigger the cleared response (should not work) ---
  std::cout << "[Controller] Enqueuing ID request to simulated slave 0x15 "
               "again (should fail)..."
            << std::endl;
  controller.enqueue(10, ebus::toVector("15070400"),
                     [](const ebus::ResultInfo& info) {
                       if (info.success) {
                         std::cout << "[Controller] ID request to 0x15 "
                                      "successful (unexpected after clear)."
                                   << std::endl;
                       } else {
                         std::cout << "[Controller] ID request to 0x15 failed "
                                      "(expected after clear): "
                                   << ebus::toString(info.result) << std::endl;
                       }
                     });
  std::this_thread::sleep_for(2s);

  // --- 12. Stop the Controller ---
  std::cout << "Stopping controller." << std::endl;
  controller.stop();

  return 0;
}
