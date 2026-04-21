/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/utils.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "app/device_manager.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"

#define ASSERT_TRUE(cond)                                                   \
  if (!(cond)) {                                                            \
    std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
    exit(1);                                                                \
  }

void test_address_tracking() {
  std::cout << "[TEST] DeviceManager Address Tracking... ";

  // Setup handler dependencies to avoid nullptr issues, though update() now
  // guards against it.
  ebus::BusConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::BusMonitor monitor;
  ebus::DeviceManager dm(&monitor);
  ebus::Bus bus(config, runtime, &request, &monitor);
  ebus::Handler handler(runtime.address, &bus, &request, &monitor);

  // Simulate traffic: Master 0x10 talks to Slave 0x15
  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x00};

  dm.update(master, slave);

  auto masters = dm.getMasters();
  auto slaves = dm.getSlaves();

  ASSERT_TRUE(masters.size() == 1);
  ASSERT_TRUE(masters[0].first == 0x10);
  ASSERT_TRUE(masters[0].second == 1);

  ASSERT_TRUE(slaves.size() == 1);
  ASSERT_TRUE(slaves[0].first == 0x15);
  ASSERT_TRUE(slaves[0].second == 1);

  auto observed = dm.getObservedSlaves();

  ASSERT_TRUE(observed[0x10] == 0);
  ASSERT_TRUE(observed[0x15] == 1);

  std::cout << "PASSED" << std::endl;
}

void test_device_update() {
  std::cout << "[TEST] DeviceManager Device Update... ";

  // Handler needed for filtering own address logic
  ebus::BusConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::BusMonitor monitor;
  ebus::DeviceManager dm(&monitor);
  ebus::Bus bus(config, runtime, &request, &monitor);
  ebus::Handler handler(runtime.address, &bus, &request, &monitor);

  // Vaillant Identification Response (Slave 0x08)
  // 07 04 response with Vaillant Manuf ID (0xB5)
  std::vector<uint8_t> master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x50, 0x4d, 0x53, 0x30,
                                0x30, 0x01, 0x07, 0x43, 0x02};

  dm.update(master, slave);

  auto devices = dm.getDeviceInfo();
  // Should have created device at 0x08
  ASSERT_TRUE(devices.size() == 1);
  ASSERT_TRUE(devices[0].slave_address == 0x08);

  // Verify Vendor Scan Commands are generated
  auto cmds = dm.vendorScanCommands();
  // Vaillant device should generate 4 commands (0x24-0x27)
  ASSERT_TRUE(cmds.size() == 4);
  // Check first command targets 0x08
  ASSERT_TRUE(cmds[0][0] == 0x08);

  std::cout << "PASSED" << std::endl;
}

void test_create_scan_commands() {
  std::cout << "[TEST] DeviceManager Create Scan Commands... ";

  // Use handler with address 0xff
  ebus::BusConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::BusMonitor monitor;
  ebus::DeviceManager dm(&monitor);
  ebus::Bus bus(config, runtime, &request, &monitor);
  ebus::Handler handler(runtime.address, &bus, &request, &monitor);

  std::vector<std::string> inputs = {"08", "15", "50"};
  auto cmds = dm.createScanCommands(inputs);

  ASSERT_TRUE(cmds.size() == 3);
  // Verify command structure: [Address, 07, 04, 00]
  ASSERT_TRUE(cmds[0][0] == 0x08 && cmds[0][1] == 0x07);
  ASSERT_TRUE(cmds[1][0] == 0x15);
  ASSERT_TRUE(cmds[2][0] == 0x50);

  std::cout << "PASSED" << std::endl;
}

int main() {
  test_address_tracking();
  test_device_update();
  test_create_scan_commands();

  std::cout << "\nAll devicemanager tests passed!" << std::endl;

  return EXIT_SUCCESS;
}