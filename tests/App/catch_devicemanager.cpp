/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

#include "App/DeviceManager.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "TestUtils.hpp"
#include "Utils/Common.hpp"

TEST_CASE("DeviceManager: Address Tracking", "[app][devicemanager]") {
  ebus::DeviceManager dm;

  ebus::busConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(runtime.address, &bus, &request);

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x00};

  dm.update(master, slave);

  auto masters = dm.getMasters();
  auto slaves = dm.getSlaves();

  REQUIRE(masters.count(0x10) > 0);
  REQUIRE(slaves.count(0x15) > 0);
  REQUIRE(dm.getObservedSlaves().count(0x15) > 0);
}

TEST_CASE("DeviceManager: Device Update", "[app][devicemanager]") {
  ebus::DeviceManager dm;

  ebus::busConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(runtime.address, &bus, &request);

  std::vector<uint8_t> master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x50, 0x4d, 0x53, 0x30,
                                0x30, 0x01, 0x07, 0x43, 0x02};

  dm.update(master, slave);

  auto devices = dm.getDeviceInfo();
  REQUIRE(devices.size() == 1);
  REQUIRE(devices[0].slave == 0x08);

  auto cmds = dm.vendorScanCommands();
  REQUIRE(cmds.size() == 4);
  REQUIRE(cmds[0][0] == 0x08);
}

TEST_CASE("DeviceManager: Create Scan Commands", "[app][devicemanager]") {
  ebus::DeviceManager dm;

  ebus::busConfig config{.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};
  ebus::Request request;
  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(runtime.address, &bus, &request);

  std::vector<std::string> inputs = {"08", "15", "50"};
  auto cmds = dm.createScanCommands(inputs);

  REQUIRE(cmds.size() == 3);
  REQUIRE(cmds[0][0] == 0x08);
  REQUIRE(cmds[0][1] == 0x07);
  REQUIRE(cmds[1][0] == 0x15);
  REQUIRE(cmds[2][0] == 0x50);
}
