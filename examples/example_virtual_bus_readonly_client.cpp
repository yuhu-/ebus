/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Example: Virtual Bus eBUS Simulation with Read-Only Client
 * This example demonstrates how to run multiple controllers on a virtual bus.
 */

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <ebus.hpp>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
  // --- 1. Configuration for Device A (Traffic Generator) ---
  // This device will act as a Master at address 0x01.
  ebus::EbusConfig configA;
  configA.runtime.address = 0x01;
  configA.bus.simulate = true;
  configA.runtime.enable_syn = true;  // One device should act as SYN generator

  ebus::Controller deviceA(configA);

  // --- 2. Configuration for Device B (Boiler Emulator) ---
  // This device acts as Master 0x10 (Slave 0x15).
  ebus::EbusConfig configB;
  configB.runtime.address = 0x10;
  configB.bus.simulate = true;

  ebus::Controller deviceB(configB);

  // Mocking an external client connection (ebusd).
  // In a real app, you would accept a TCP connection and pass the file
  // descriptor. Here we just show the call. int ebusd_socket_fd = ...;
  // deviceB.addClient(ebusd_socket_fd, ebus::ClientType::Enhanced);

  // --- 3. Setup a Read-Only Client (Passive Logger) ---
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 1;
  // The controller takes ownership of sv[0]. We use sv[1] to monitor.
  fcntl(sv[1], F_SETFL, O_NONBLOCK);
  deviceA.addClient(sv[0], ebus::ClientType::read_only);

  // --- 4. Logic for Device B: Periodic Traffic ---
  // Enqueue a broadcast message every 5 seconds.
  // Example: Broadcasting of a temperature of 9.25°C
  // * Master 0x10 -> Broadcast (0xfe),
  // * Vaillant Service (0xb5 0x16 0x03 0x01),
  // * Data: 9.25°C - DATA2B -> 0x40, 0x09
  std::vector<uint8_t> broadcastMsg = {0xfe, 0xb5, 0x16, 0x03,
                                       0x01, 0x40, 0x09};
  deviceB.addPollItem(10, broadcastMsg, 5s, [](ebus::ByteView byte_view) {
    std::cout << "[Device B] Periodic broadcast sent." << std::endl;
  });

  // Try to enqueue a faulty broadcast message every 7 seconds.
  // This library offers several helper functions, such as...ebus::to_vector("")
  deviceB.addPollItem(
      15, ebus::toVector("feb5160301"), 7s, [](ebus::ByteView byte_view) {
        std::cout << "[Device B] Periodic faulty broadcast sent." << std::endl;
      });

  // --- 5. Logic for Device A: Decoding of a received message ---
  // We set a callback to decode the received message from DeviceB.
  deviceA.setTelegramCallback(
      [](ebus::MessageType message_type, ebus::TelegramType telegram_type,
         ebus::ByteView master_view, ebus::ByteView slave_view) {
        if (telegram_type == ebus::TelegramType::broadcast) {
          if (ebus::matches(master_view, {0xfe, 0xb5, 0x16, 0x03, 0x01}, 1))
            std::cout << "[Device A] Observed broadcast from "
                      << ebus::toString(master_view[0]) << " with data: "
                      << ebus::toString(
                             *ebus::decode(ebus::DataType::data2b,
                                           ebus::range(master_view, 6, 2)),
                             "°C")
                      << std::endl;
        }
      });

  // --- 6. Add a error callback handler to Device B ---
  deviceB.setErrorCallback([](std::string_view error_message,
                              ebus::ByteView master_view,
                              ebus::ByteView slave_view) {
    std::cout << "[Device B] Error message " << error_message << " master: '"
              << ebus::toString(master_view) << "' slave: '"
              << ebus::toString(slave_view) << "'" << std::endl;
  });

  // --- 7. Start the simulation ---
  std::cout << "Starting simulation on virtual bus..." << std::endl;
  deviceA.start();
  deviceB.start();

  // Run for a while to observe traffic
  for (int i = 0; i < 30; ++i) {
    std::this_thread::sleep_for(1s);

    // Poll the Read-Only client's socket (on deviceA)
    uint8_t logBuf[256];
    ssize_t n = read(sv[1], logBuf, sizeof(logBuf));
    if (n > 0) {
      // If you see 0xaa here, the virtual connection is working.
      // If you see nothing, the VirtualLine is isolated.
      std::cout << "[DeviceA ReadOnly] Logged " << n << " bytes: "
                << ebus::byteToHex(std::vector<uint8_t>(logBuf, logBuf + n))
                << std::endl;
    }

    // Print metrics to see bus health
    auto metrics = deviceB.getMetrics();
    std::cout << "Bus Quality: " << metrics.quality << "%" << std::endl;
  }

  // Print metrics to see bus health
  auto metrics = deviceB.getMetrics();
  std::cout << "devieB Metrics: " << ebus::toJson(metrics) << std::endl;

  // --- 8. Stop the simulation ---
  std::cout << "Stopping simulation on virtual bus..." << std::endl;

  deviceA.stop();
  deviceB.stop();
  close(sv[1]);
  return 0;
}
