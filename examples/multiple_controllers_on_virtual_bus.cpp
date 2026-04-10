/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Example: Multi-device eBUS Simulation
 * This example demonstrates how to run multiple controllers on a virtual bus.
 */

#include <ebus/Controller.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std::chrono_literals;

int main() {
  // --- 1. Configuration for Device A (Traffic Generator) ---
  // This device will act as a Master at address 0x31.
  ebus::ebusConfig configA;
  configA.runtime.address = 0x01;
  configA.runtime.enable_syn = true;  // One device should act as SYN generator
  configA.bus.simulate = true;

  ebus::Controller deviceA(configA);

  // --- 2. Configuration for Device B (Boiler Emulator) ---
  // This device acts as Master 0x10 (Slave 0x15).
  ebus::ebusConfig configB;
  configB.runtime.address = 0x10;
  configB.bus.simulate = true;

  ebus::Controller deviceB(configB);

  // --- 2.1 Setup a Read-Only Client (Passive Logger) ---
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 1;
  // The controller takes ownership of sv[0]. We use sv[1] to monitor.
  fcntl(sv[1], F_SETFL, O_NONBLOCK);
  deviceB.addClient(sv[0], ebus::ClientType::ReadOnly);

  // --- 3. Logic for Device A: Periodic Traffic ---
  // Enqueue a broadcast message every 5 seconds.
  // Example: Master 31 -> Broadcast (FE), Service 05 03 (Data: temp 21.5C)
  std::vector<uint8_t> broadcastMsg = {0xfe, 0x05, 0x03, 0x01, 0xAC};
  deviceA.addPollItem(
      10, broadcastMsg, 5s, [](const std::vector<uint8_t>& data) {
        std::cout << "[Device A] Periodic broadcast sent." << std::endl;
      });

  // --- 4. Logic for Device B: Reactive Responses & ebusd Bridge ---
  // We set a callback to react when someone scans Device B.
  deviceB.setTelegramCallback([](ebus::MessageType mType,
                                 ebus::TelegramType tType,
                                 const std::vector<uint8_t>& master,
                                 const std::vector<uint8_t>& slave) {
    // If we see an identification request (07 04) for our slave address (0x15)
    if (master.size() > 1 && master[1] == 0x15) {
      std::cout << "[Device B] Observed scan request for our address!"
                << std::endl;
    }
  });

  // Mocking an external client connection (ebusd).
  // In a real app, you would accept a TCP connection and pass the file
  // descriptor. Here we just show the call. int ebusd_socket_fd = ...;
  // deviceB.addClient(ebusd_socket_fd, ebus::ClientType::Enhanced);

  // --- 5. Start the simulation ---
  std::cout << "Starting simulation on virtual bus..." << std::endl;
  deviceA.start();
  deviceB.start();

  // Run for a while to observe traffic
  for (int i = 0; i < 30; ++i) {
    std::this_thread::sleep_for(1s);

    // Poll the Read-Only client's socket (on deviceB)
    uint8_t logBuf[256];
    ssize_t n = read(sv[1], logBuf, sizeof(logBuf));
    if (n > 0) {
      // If you see 0xAA here, the virtual connection is working.
      // If you see nothing, the VirtualLine is isolated.
      std::cout << "[DeviceB ReadOnly] Logged " << n << " bytes: "
                << ebus::byte_2_hex(std::vector<uint8_t>(logBuf, logBuf + n)) << std::endl;
    }

    // Manually trigger a scan from Device A to see Device B react
    if (i == 2) {
      std::cout << "[Device A] Initiating manual scan of address 0x15..."
                << std::endl;
      deviceA.scanAddress(0x15);
    }

    // Print metrics to see bus health
    auto metrics = deviceB.getMetrics();
    if (metrics.count("bus.quality")) {
      std::cout << "Bus Quality: " << metrics["bus.quality"].last << "%"
                << std::endl;
    }
  }

  deviceA.stop();
  deviceB.stop();
  close(sv[1]);
  return 0;
}
