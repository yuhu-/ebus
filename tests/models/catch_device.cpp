/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <string>
#include <vector>

#include "models/device.hpp"
#include "utils/common.hpp"

TEST_CASE("Device: createScanCommand", "[models][device]") {
  auto cmd = ebus::Device::createScanCommand(0x15);
  REQUIRE(cmd.size() == 4);
  REQUIRE(cmd[0] == 0x15);
  REQUIRE(cmd[1] == 0x07);
  REQUIRE(cmd[2] == 0x04);
  REQUIRE(cmd[3] == 0x00);
}

TEST_CASE("Device: update parsing", "[models][device]") {
  ebus::Device dev;

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xda, 0x45, 0x53, 0x50, 0x44,
                                0x41, 0x07, 0x02, 0x06, 0x03};

  dev.update(master, slave);

  auto info = dev.getDeviceInfo();
  REQUIRE(info.manufacturer == 0xda);

  std::string unitIdStr(info.unit_id.begin(), info.unit_id.end());
  REQUIRE(unitIdStr == "ESPDA");
}

TEST_CASE("Device: Vaillant vendor scan commands", "[models][device]") {
  ebus::Device dev;

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00};

  dev.update(master, slave);

  auto cmds = dev.createVendorScanCommands();
  REQUIRE(cmds.size() == 4);

  REQUIRE(cmds[0][0] == 0x15);
  REQUIRE(cmds[0][1] == 0xb5);
  REQUIRE(cmds[0][4] == 0x24);
}

TEST_CASE("Device: Vaillant full identification", "[models][device]") {
  ebus::Device dev;

  std::vector<uint8_t> m1 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x24};
  std::vector<uint8_t> s1 = {0x09, 0x00, 0x32, 0x31, 0x31,
                             0x31, 0x32, 0x36, 0x33, 0x30};

  std::vector<uint8_t> m2 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x25};
  std::vector<uint8_t> s2 = {0x09, 0x36, 0x37, 0x38, 0x32,
                             0x3c, 0x3c, 0x3c, 0x3c, 0x30};

  std::vector<uint8_t> m3 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x26};
  std::vector<uint8_t> s3 = {0x09, 0x39, 0x30, 0x37, 0x30,
                             0x30, 0x35, 0x32, 0x37, 0x36};

  std::vector<uint8_t> m4 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x27};
  std::vector<uint8_t> s4 = {0x09, 0x4e, 0x34, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00};

  dev.update(m1, s1);
  dev.update(m2, s2);
  dev.update(m3, s3);
  dev.update(m4, s4);

  REQUIRE(dev.getSlave() == 0x52);

  const auto& v24 = dev.getVendorData(0x24);
  const auto& v25 = dev.getVendorData(0x25);
  const auto& v26 = dev.getVendorData(0x26);
  const auto& v27 = dev.getVendorData(0x27);

  REQUIRE(v24 == s1);
  REQUIRE(v25 == s2);
  REQUIRE(v26 == s3);
  REQUIRE(v27 == s4);
}