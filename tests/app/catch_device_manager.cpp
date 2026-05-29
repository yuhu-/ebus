/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <vector>

#include "app/device_manager.hpp"
#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "platform/bus.hpp"
#include "test_utils.hpp"

using namespace ebus::detail;

TEST_CASE("DeviceManager: Address Tracking", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);

  dm.setOwnAddress(runtime.address);

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x00};

  dm.update(master, slave);

  std::bitset<256> observed;
  dm.getObservedSlaves(observed);

  REQUIRE(observed[0x10] == 0);
  REQUIRE(observed[0x15] == 1);
}

TEST_CASE("DeviceManager: Device Update", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);

  dm.setOwnAddress(runtime.address);

  std::vector<uint8_t> master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x50, 0x4d, 0x53, 0x30,
                                0x30, 0x01, 0x07, 0x43, 0x02};

  dm.update(master, slave);

  std::vector<ebus::DeviceInfo> devices;
  dm.fetchDeviceInfo([&](const ebus::DeviceInfo& info) {
    devices.push_back(info);
  });

  REQUIRE(devices.size() == 1);
  REQUIRE(devices[0].slave_address == 0x08);

  auto cmds = dm.vendorScanCommands();
  REQUIRE(cmds.size() == 4);
  REQUIRE(cmds[0][0] == 0x08);
}

TEST_CASE("DeviceManager: Create Scan Commands", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor monitor;
  DeviceManager dm(&monitor);
  platform::Bus bus(config, runtime, &request, &monitor);
  Handler handler(runtime.address, &bus, &request, &monitor);

  std::vector<std::string> inputs = {"08", "15", "50"};
  auto cmds = dm.createScanCommands(inputs);

  REQUIRE(cmds.size() == 3);
  REQUIRE(cmds[0][0] == 0x08);
  REQUIRE(cmds[0][1] == 0x07);
  REQUIRE(cmds[1][0] == 0x15);
  REQUIRE(cmds[2][0] == 0x50);
}
