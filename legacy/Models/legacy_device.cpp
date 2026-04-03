/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <string>
#include <vector>

#include "Models/Device.hpp"
#include "Utils/Common.hpp"

// Helper macro for simple assertions
#define ASSERT_EQ(val, expected)                                            \
  if ((val) != (expected)) {                                                \
    std::cerr << "FAIL: " << #val << " (" << (val) << ") != " << (expected) \
              << " at line " << __LINE__ << std::endl;                      \
    exit(1);                                                                \
  }

void test_scan_command() {
  std::cout << "[TEST] Device::createScanCommand... ";
  auto cmd = ebus::Device::createScanCommand(0x15);
  // Expected: Slave(15) + 07 04 00
  ASSERT_EQ(cmd.size(), 4);
  ASSERT_EQ(cmd[0], 0x15);
  ASSERT_EQ(cmd[1], 0x07);
  ASSERT_EQ(cmd[2], 0x04);
  ASSERT_EQ(cmd[3], 0x00);
  std::cout << "PASSED" << std::endl;
}

void test_parsing() {
  std::cout << "[TEST] Device::update parsing... ";
  ebus::Device dev;

  // Master: QQ ZZ 07 04 00
  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};

  // Slave Response (Identification):
  // NN=0A, Man=DA, ID=ESPDA, SW=07 02, HW=06 03
  std::vector<uint8_t> slave = {0x0a, 0xda, 0x45, 0x53, 0x50, 0x44,
                                0x41, 0x07, 0x02, 0x06, 0x03};

  dev.update(master, slave);

  auto info = dev.getDeviceInfo();
  ASSERT_EQ(info.manufacturer, 0xda);

  // Check Unit ID string conversion
  std::string unitIdStr(info.unitID.begin(), info.unitID.end());
  if (unitIdStr != "ESPDA") {
    std::cerr << "UnitID mismatch: " << unitIdStr << std::endl;
    exit(1);
  }

  std::cout << "PASSED" << std::endl;
}

void test_vaillant() {
  std::cout << "[TEST] Device::createVendorScanCommands (Vaillant)... ";
  ebus::Device dev;

  std::vector<uint8_t> master = {0x10, 0x15, 0x07, 0x04, 0x00};
  // Slave with 0xB5 (Vaillant) manufacturer
  std::vector<uint8_t> slave = {0x0a, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00};

  dev.update(master, slave);

  auto cmds = dev.createVendorScanCommands();
  // Vaillant device should trigger 4 vendor specific scans (24, 25, 26, 27)
  ASSERT_EQ(cmds.size(), 4);

  // Check first command structure: Slave(15) B5 09 01 24
  ASSERT_EQ(cmds[0][0], 0x15);
  ASSERT_EQ(cmds[0][1], 0xb5);
  ASSERT_EQ(cmds[0][4], 0x24);

  std::cout << "PASSED" << std::endl;
}

void test_vaillant_identification() {
  std::cout << "[TEST] Device::update Vaillant full identification... ";
  ebus::Device dev;

  // Example data from Vaillant device (Address 0x52)
  // Sequence 1: 30 52 b5 09 01 24 ...
  std::vector<uint8_t> m1 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x24};
  std::vector<uint8_t> s1 = {0x09, 0x00, 0x32, 0x31, 0x31,
                             0x31, 0x32, 0x36, 0x33, 0x30};

  // Sequence 2: 30 52 b5 09 01 25 ...
  std::vector<uint8_t> m2 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x25};
  std::vector<uint8_t> s2 = {0x09, 0x36, 0x37, 0x38, 0x32,
                             0x3c, 0x3c, 0x3c, 0x3c, 0x30};

  // Sequence 3: 30 52 b5 09 01 26 ...
  std::vector<uint8_t> m3 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x26};
  std::vector<uint8_t> s3 = {0x09, 0x39, 0x30, 0x37, 0x30,
                             0x30, 0x35, 0x32, 0x37, 0x36};

  // Sequence 4: 30 52 b5 09 01 27 ...
  std::vector<uint8_t> m4 = {0x30, 0x52, 0xb5, 0x09, 0x01, 0x27};
  std::vector<uint8_t> s4 = {0x09, 0x4e, 0x34, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00};

  dev.update(m1, s1);
  dev.update(m2, s2);
  dev.update(m3, s3);
  dev.update(m4, s4);

  // Verify slave address was captured
  ASSERT_EQ(dev.getSlave(), 0x52);

  // Verify stored data blocks match the input slave responses
  const auto& v24 = dev.getVendorData(0x24);
  const auto& v25 = dev.getVendorData(0x25);
  const auto& v26 = dev.getVendorData(0x26);
  const auto& v27 = dev.getVendorData(0x27);

  if (v24 != s1 || v25 != s2 || v26 != s3 || v27 != s4) {
    std::cerr << "FAIL: Stored vendor data does not match input sequences"
              << std::endl;
    exit(1);
  }

  std::cout << "PASSED" << std::endl;
}

int main() {
  test_scan_command();
  test_parsing();
  test_vaillant();
  test_vaillant_identification();

  std::cout << "\nAll device tests passed!" << std::endl;

  return EXIT_SUCCESS;
}