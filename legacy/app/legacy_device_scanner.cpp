/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/utils.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"

#define ASSERT_TRUE(cond)                                                   \
  if (!(cond)) {                                                            \
    std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << std::endl; \
    exit(1);                                                                \
  }

void test_manual_and_stop() {
  std::cout << "[TEST] DeviceScanner Manual Scan & Stop... ";
  ebus::DeviceScanner scanner(0xff, nullptr);

  scanner.scanAddress(0x15);
  scanner.scanAddress(0x16);
  ASSERT_TRUE(scanner.isScanning());

  auto cmd1 = scanner.nextCommand();
  ASSERT_TRUE(!cmd1.empty() && cmd1[0] == 0x15);

  scanner.stop();
  ASSERT_TRUE(!scanner.isScanning());
  ASSERT_TRUE(scanner.nextCommand().empty());

  std::cout << "PASSED" << std::endl;
}

void test_priority() {
  std::cout << "[TEST] DeviceScanner Priority (Manual > Full > Startup)... ";
  ebus::DeviceManager dm;
  ebus::DeviceScanner scanner(0xff, &dm);

  // Enable all scan types
  scanner.setFullScan(true);
  scanner.setScanOnStartup(true);
  scanner.scanAddress(0x50);  // Manual scan

  // 1. Expect Manual Scan first
  auto cmd1 = scanner.nextCommand();
  ASSERT_TRUE(!cmd1.empty() && cmd1[0] == 0x50);

  // 2. Manual queue is empty. Expect Full Scan next (first valid slave is 0x02)
  auto cmd2 = scanner.nextCommand();
  ASSERT_TRUE(!cmd2.empty() && cmd2[0] == 0x02);

  // 3. Stop full scan, now startup scan should be next (but it's empty)
  scanner.setFullScan(false);
  scanner.setInitialScanDelay(std::chrono::seconds(0));
  ASSERT_TRUE(
      scanner.nextCommand().empty());  // Startup scan runs but finds no devices

  std::cout << "PASSED" << std::endl;
}

void test_startup_scan_logic() {
  std::cout << "[TEST] DeviceScanner Startup Scan Logic... ";
  ebus::BusMonitor monitor;
  ebus::DeviceManager dm(&monitor);
  dm.setOwnAddress(0xff);
  ebus::DeviceScanner scanner(0xff, &dm);

  // Configure for fast testing
  scanner.setInitialScanDelay(std::chrono::seconds(0));
  scanner.setStartupScanInterval(std::chrono::seconds(0));
  scanner.setMaxStartupScans(2);

  // Enable startup scan
  scanner.setScanOnStartup(true);
  ASSERT_TRUE(scanner.isScanOnStartup());

  // Iteration 1: No devices observed yet. Should return no command.
  ASSERT_TRUE(scanner.nextCommand().empty());

  // Add an observed device
  dm.update(ebus::ByteView({0x10, 0x15, 0x00}),
            ebus::ByteView({0x00}));  // Master 10 -> Slave 15

  // Iteration 2: Should now find and queue a command for 0x15.
  auto cmd1 = scanner.nextCommand();
  ASSERT_TRUE(!cmd1.empty() && cmd1[0] == 0x15);
  ASSERT_TRUE(
      scanner.nextCommand().empty());  // Queue for this iteration is empty

  // Iteration 3: We have reached max scans (2), so it should now be disabled.
  ASSERT_TRUE(scanner.nextCommand().empty());
  ASSERT_TRUE(!scanner.isScanOnStartup());

  std::cout << "PASSED" << std::endl;
}

void test_timing_and_vendor_integration() {
  std::cout << "[TEST] DeviceScanner Timing & Vendor Scans... ";
  ebus::BusMonitor monitor;
  ebus::DeviceManager dm(&monitor);
  dm.setOwnAddress(0x10);
  ebus::DeviceScanner scanner(0x10, &dm);

  // 1. Identify a Vaillant device at 0x08
  std::vector<uint8_t> m = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> s = {0x0a, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00};
  dm.update(m, s);

  // 2. Setup Startup Scan with a short delay
  scanner.setInitialScanDelay(std::chrono::seconds(1));
  scanner.setScanOnStartup(true);

  // 3. Immediately ask for command - should be empty (delay active)
  ASSERT_TRUE(scanner.nextCommand().empty());

  // 4. Wait for delay to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // 5. Next command should be the standard scan for 0x08
  auto cmd1 = scanner.nextCommand();
  ASSERT_TRUE(!cmd1.empty() && cmd1[0] == 0x08 && cmd1[1] == 0x07);

  // 6. Following command should be a Vaillant vendor scan (B5 09)
  auto cmd2 = scanner.nextCommand();
  ASSERT_TRUE(!cmd2.empty() && cmd2[0] == 0x08 && cmd2[1] == 0xb5);

  std::cout << "PASSED" << std::endl;
}

int main() {
  test_manual_and_stop();
  test_priority();
  test_startup_scan_logic();
  test_timing_and_vendor_integration();

  std::cout << "\nAll devicescanner tests passed!" << std::endl;

  return EXIT_SUCCESS;
}