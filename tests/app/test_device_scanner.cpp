/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <thread>
#include <vector>

#include "app/device_manager.hpp"
#include "app/device_scanner.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "platform/system.hpp"

using namespace ebus::detail;

TEST_CASE("DeviceScanner: Manual Scan & Stop", "[app][devicescanner]") {
  DeviceManager dm;
  DeviceScanner scanner(0xff, &dm);

  scanner.scanAddress(0x15);
  scanner.scanAddress(0x16);
  REQUIRE(scanner.isScanning());

  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x15);

  scanner.stop();
  REQUIRE(!scanner.isScanning());
  REQUIRE(scanner.nextCommand().empty());
}

TEST_CASE("DeviceScanner: Priority (Manual > Full > Startup)",
          "[app][devicescanner]") {
  DeviceManager dm;
  DeviceScanner scanner(0xff, &dm);

  scanner.setInitialScanDelay(0);
  scanner.initFullScan(true);
  scanner.setScanOnStartup(true);
  scanner.scanAddress(0x50);  // Manual scan

  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x50);

  auto cmd2 = scanner.nextCommand();
  REQUIRE(!cmd2.empty());
  REQUIRE(cmd2[0] == 0x02);

  scanner.initFullScan(false);
  scanner.setInitialScanDelay(0);
  REQUIRE(scanner.nextCommand().empty());
}

TEST_CASE("DeviceScanner: Startup Scan Logic", "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  dm.setOwnAddress(0xff);
  DeviceScanner scanner(0xff, &dm);

  scanner.setInitialScanDelay(0);
  scanner.setStartupScanInterval(0);
  scanner.setMaxStartupScans(2);

  scanner.setScanOnStartup(true);
  REQUIRE(scanner.isScanOnStartup());

  REQUIRE(scanner.nextCommand().empty());

  dm.update(ebus::ByteView({0x10, 0x15, 0x00}), ebus::ByteView({0x00}));

  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x15);
  REQUIRE(scanner.nextCommand().empty());

  REQUIRE(scanner.nextCommand().empty());
  REQUIRE(!scanner.isScanOnStartup());
}

TEST_CASE("DeviceScanner: Timing & Vendor Scans", "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  dm.setOwnAddress(0x10);
  DeviceScanner scanner(0x10, &dm);

  std::vector<uint8_t> m = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> s = {0x0a, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00};
  dm.update(m, s);

  scanner.setInitialScanDelay(1);
  scanner.setScanOnStartup(true);

  REQUIRE(scanner.nextCommand().empty());

  platform::sleepMilli(1100);

  auto cmd1 = scanner.nextCommand();
  // Since the device at 0x08 was pre-identified in the DeviceManager via
  // dm.update(), the generator should skip the 07 04 identification and return
  // the first vendor-specific command (0xb5).
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x08);
  REQUIRE(cmd1[1] == 0xb5);
}

TEST_CASE("DeviceScanner: Transition 07 04 to Vendor Scans",
          "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  dm.setOwnAddress(0xff);
  DeviceScanner scanner(0xff, &dm);

  uint8_t target = 0x08;
  scanner.scanAddress(target);

  // 1. First call should be standard ID scan (07 04)
  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == target);
  REQUIRE(cmd1[1] == 0x07);
  REQUIRE(cmd1[2] == 0x04);

  // 2. Simulate receiving 07 04 response (Vaillant)
  // Master: [Source, Target, 07, 04, 00]
  // Slave: [NN, Manufacturer, ...]
  std::vector<uint8_t> master_id = {0x10, target, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave_id = {0x0a, 0xb5, 'V', 'A', 'I', 'L',
                                   '0',  '0',  '0', '0', '0'};
  dm.update(master_id, slave_id);
  scanner.onScanResult(target, true);  // Trigger next step in chain

  REQUIRE(dm.isIdentified(target));

  // 3. Next call should transition to vendor-specific scan (Vaillant 24h)
  auto cmd2 = scanner.nextCommand();
  REQUIRE(!cmd2.empty());
  REQUIRE(cmd2[0] == target);
  REQUIRE(cmd2[1] == 0xb5);
  REQUIRE(cmd2[2] == 0x09);
  REQUIRE(cmd2[3] == 0x01);
  REQUIRE(cmd2[4] == 0x24);

  // 4. Simulate completing vendor 24h
  dm.update(
      ebus::ByteView({0x10, target, 0xb5, 0x09, 0x01, 0x24}),
      ebus::ByteView({0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
  scanner.onScanResult(target, true);  // Trigger next vendor command

  // 5. Next call should be next vendor command (25h)
  auto cmd3 = scanner.nextCommand();
  REQUIRE(!cmd3.empty());
  REQUIRE(cmd3[4] == 0x25);
}

TEST_CASE("DeviceScanner: Cooldown for Failed Scan", "[app][devicescanner]") {
  DeviceManager dm;
  DeviceScanner scanner(0xff, &dm);

  uint8_t target = 0x15;
  scanner.scanAddress(target);

  // 1. Initial attempt
  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == target);

  // 2. Report failure
  scanner.onScanResult(target, false);
  REQUIRE(scanner.fetchStatus().failed_scans == 1);

  // 3. Next call should return empty because target is on cooldown
  auto cmd2 = scanner.nextCommand();
  REQUIRE(cmd2.empty());
}

TEST_CASE("DeviceScanner: Full Scan Skip Own Address", "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  // Controller address 0x12 -> Slave address 0x17
  uint8_t own_master = 0x12;
  uint8_t own_slave = 0x17;
  DeviceScanner scanner(own_master, &dm);

  scanner.initFullScan(true);

  // We manually advance the full_scan_address_ state by calling nextCommand
  // until we are near our own address. Slaves are 0x00, 0x01, 0x02...
  // We skip some steps to reach 0x35.
  for (int i = 0; i < own_slave; ++i) {
    auto cmd = scanner.nextCommand();
    REQUIRE(!cmd.empty());
    // Ensure we never get our own slave address
    REQUIRE(cmd[0] != own_slave);
  }
}

TEST_CASE("DeviceScanner: Startup Scan Stops After Max Iterations",
          "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  // Use master 0xff so its slave 0x04 doesn't conflict with 0x15
  DeviceScanner scanner(0xff, &dm);

  // Configure for a single iteration with no delay
  scanner.setInitialScanDelay(0);
  scanner.setStartupScanInterval(0);
  scanner.setMaxStartupScans(1);
  scanner.setScanOnStartup(true);

  // Observe one device (master only via broadcast) so there is work to do.
  // Using broadcast prevents generating tasks for a slave target, ensuring
  // the generator has exactly one task (0x36) to process.
  dm.update(ebus::ByteView({0x31, 0xfe, 0x00}), ebus::ByteView({0x00}));

  // 1. First iteration: should return a command for the observed master (0x36)
  auto cmd = scanner.nextCommand();
  REQUIRE(!cmd.empty());
  REQUIRE(cmd[0] == 0x36);

  // 2. Clear generator state for that address and finish iteration
  REQUIRE(scanner.nextCommand().empty());

  // 3. Since MaxStartupScans is 1, the scanner should now disable itself
  // even if the interval is 0.
  REQUIRE(scanner.nextCommand().empty());
  REQUIRE(scanner.isScanOnStartup() == false);
}