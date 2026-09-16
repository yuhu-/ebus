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

  // Only the addressed slave with response data becomes a device entry.
  // The source master's slave counterpart (0x15) stays observed-only:
  // no telegram ever involves it, so there is nothing to identify.
  REQUIRE(devices.size() == 1);
  REQUIRE(devices[0].slave_address == 0x08);
  REQUIRE(device_manager.isIdentified(0x08));
  REQUIRE(!device_manager.isIdentified(0x15));

  std::bitset<256> observed;
  device_manager.getObservedSlaves(observed);
  REQUIRE(observed[0x15] == 1);  // still scanned, just not listed

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

TEST_CASE("DeviceManager: broadcast storm creates no phantom entries",
          "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // 0x10 broadcast master traffic: slave side carries no data.
  std::vector<uint8_t> master = {0x10, 0xfe, 0xb5, 0x16, 0x03,
                                 0x05, 0x01, 0x02, 0x03};
  std::vector<uint8_t> empty;
  for (int i = 0; i < 50; ++i) {
    device_manager.update(master, empty);
  }

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.empty());

  // ...but the counterpart stays observed for scan targeting.
  std::bitset<256> observed;
  device_manager.getObservedSlaves(observed);
  REQUIRE(observed[0x15] == 1);
}

TEST_CASE("DeviceManager: own transmissions create no self entries",
          "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0x30};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // Our own polls (master 0x30) must not allocate our slave side (0x35).
  std::vector<uint8_t> master = {0x30, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> empty;
  for (int i = 0; i < 20; ++i) {
    device_manager.update(master, empty);
  }

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.empty());
}

TEST_CASE("DeviceManager: unanswered targets create no entries",
          "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // Scan probe with no response: target stays unlisted.
  std::vector<uint8_t> master = {0xff, 0x09, 0x07, 0x04, 0x00};
  std::vector<uint8_t> empty;
  device_manager.update(master, empty);

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.empty());
  REQUIRE(!device_manager.isIdentified(0x09));
}

TEST_CASE("DeviceManager: responding targets allocate unidentified",
          "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // Non-identification response data: entry exists but stays unidentified
  // (real-but-silent participant, e.g. room controller).
  std::vector<uint8_t> master = {0x10, 0x23, 0x05, 0x03, 0x00};
  std::vector<uint8_t> slave = {0x01, 0x02};
  device_manager.update(master, slave);

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.size() == 1);
  REQUIRE(devices[0].slave_address == 0x23);
  REQUIRE(!device_manager.isIdentified(0x23));
}

TEST_CASE("DeviceManager: prune drops only unidentified entries",
          "[app][devicemanager]") {
  ebus::BusConfig config;
  ebus::RuntimeConfig runtime = {.address = 0xff};
  Request request;
  BusMonitor bus_monitor;
  DeviceManager device_manager(&bus_monitor);
  platform::Bus bus(config, runtime, &request, &bus_monitor);
  Handler handler(runtime.address, &bus, &request, &bus_monitor);

  device_manager.setOwnAddress(runtime.address);

  // Unidentified responder.
  std::vector<uint8_t> master = {0x10, 0x23, 0x05, 0x03, 0x00};
  std::vector<uint8_t> slave = {0x01, 0x02};
  device_manager.update(master, slave);

  // Identified device (0704 with Vaillant manufacturer).
  std::vector<uint8_t> id_master = {0x10, 0x08, 0x07, 0x04, 0x00};
  std::vector<uint8_t> id_slave = {0x0a, 0xb5, 0x50, 0x4d, 0x53, 0x30,
                                   0x30, 0x01, 0x07, 0x43, 0x02};
  device_manager.update(id_master, id_slave);
  REQUIRE(device_manager.isIdentified(0x08));

  device_manager.pruneUnidentified(0x23);
  device_manager.pruneUnidentified(0x08);

  std::vector<ebus::DeviceInfo> devices;
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.size() == 1);
  REQUIRE(devices[0].slave_address == 0x08);

  // Pruning an unknown address is a no-op.
  device_manager.pruneUnidentified(0x09);
  devices.clear();
  device_manager.fetchDevices(
      [&](const ebus::DeviceInfo& info) { devices.push_back(info); });
  REQUIRE(devices.size() == 1);
}
