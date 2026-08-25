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

using namespace ebus::detail;

TEST_CASE("DeviceManager: Address Tracking", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x00};

  device_manager.update(master, slave);

  std::bitset<256> observed;
  device_manager.getObservedSlaves(observed);

  REQUIRE(observed[0x10] == 0);
  REQUIRE(observed[0x15] == 1);
}

TEST_CASE("DeviceManager: Device Update", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  std::vector<uint8_t> master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x50, 0x4d, 0x53, 0x30,
                                0x30, 0x01, 0x07, 0x43, 0x02};

  device_manager.update(master, slave);

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });

  REQUIRE(devices.size() == 2);
  REQUIRE(devices[0].slave_address == 0x08);

  ebus::Sequence out_cmd;
  REQUIRE(device_manager.needsDeepScan(0x08));
  REQUIRE(device_manager.findNextPendingVendorCommand(0x08, out_cmd) == 0x08);
  REQUIRE(out_cmd[0] == 0x08);
}

TEST_CASE("DeviceManager: Manufacturer Filtering", "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // ID for a Bosch device (Manufacturer ID 0x05)
  std::vector<uint8_t> master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0x05, 0x42, 0x4f, 0x53, 0x43,
                                0x48, 0x01, 0x01, 0x01, 0x01};

  device_manager.update(master, slave);

  REQUIRE(!device_manager.needsDeepScan(0x08));
  ebus::Sequence out_cmd;
  REQUIRE(device_manager.findNextPendingVendorCommand(0x08, out_cmd) == 256);
}
