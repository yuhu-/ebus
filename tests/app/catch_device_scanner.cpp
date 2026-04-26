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
#include "test_utils.hpp"

using namespace ebus::detail;

TEST_CASE("DeviceScanner: Manual Scan & Stop", "[app][devicescanner]") {
  DeviceScanner scanner(0xff, nullptr);

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

  scanner.setFullScan(true);
  scanner.setScanOnStartup(true);
  scanner.scanAddress(0x50);  // Manual scan

  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x50);

  auto cmd2 = scanner.nextCommand();
  REQUIRE(!cmd2.empty());
  REQUIRE(cmd2[0] == 0x02);

  scanner.setFullScan(false);
  scanner.setInitialScanDelay(std::chrono::seconds(0));
  REQUIRE(scanner.nextCommand().empty());
}

TEST_CASE("DeviceScanner: Startup Scan Logic", "[app][devicescanner]") {
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  dm.setOwnAddress(0xff);
  DeviceScanner scanner(0xff, &dm);

  scanner.setInitialScanDelay(std::chrono::seconds(0));
  scanner.setStartupScanInterval(std::chrono::seconds(0));
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

  scanner.setInitialScanDelay(std::chrono::seconds(1));
  scanner.setScanOnStartup(true);

  REQUIRE(scanner.nextCommand().empty());

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto cmd1 = scanner.nextCommand();
  REQUIRE(!cmd1.empty());
  REQUIRE(cmd1[0] == 0x08);
  REQUIRE(cmd1[1] == 0x07);

  auto cmd2 = scanner.nextCommand();
  REQUIRE(!cmd2.empty());
  REQUIRE(cmd2[0] == 0x08);
  REQUIRE(cmd2[1] == 0xb5);
}