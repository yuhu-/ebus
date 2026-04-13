/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "models/device.hpp"

#include <algorithm>
#include <ebus/data_types.hpp>
#include <ebus/manufacturers.hpp>
#include <ebus/utils.hpp>

constexpr uint8_t VENDOR_VAILLANT = 0xb5;

// Identification (Service 07h 04h)
const std::vector<uint8_t> VEC_070400 = {0x07, 0x04, 0x00};

// Vaillant identification (Service B5h 09h 24h-27h)
const std::vector<uint8_t> VEC_b5090124 = {0xb5, 0x09, 0x01, 0x24};
const std::vector<uint8_t> VEC_b5090125 = {0xb5, 0x09, 0x01, 0x25};
const std::vector<uint8_t> VEC_b5090126 = {0xb5, 0x09, 0x01, 0x26};
const std::vector<uint8_t> VEC_b5090127 = {0xb5, 0x09, 0x01, 0x27};

uint8_t ebus::Device::getSlave() const { return slave_; }

void ebus::Device::update(const std::vector<uint8_t>& master,
                          const std::vector<uint8_t>& slave) {
  slave_ = master[1];
  if (ebus::matches(master, VEC_070400, 2))
    vec_070400_ = slave;
  else if (ebus::matches(master, VEC_b5090124, 2))
    vec_b5090124_ = slave;
  else if (ebus::matches(master, VEC_b5090125, 2))
    vec_b5090125_ = slave;
  else if (ebus::matches(master, VEC_b5090126, 2))
    vec_b5090126_ = slave;
  else if (ebus::matches(master, VEC_b5090127, 2))
    vec_b5090127_ = slave;
}

std::vector<uint8_t> ebus::Device::getIdentificationData() const {
  return vec_070400_;
}

std::vector<uint8_t> ebus::Device::getVendorData(uint8_t sub) const {
  if (sub == 0x24) return vec_b5090124_;
  if (sub == 0x25) return vec_b5090125_;
  if (sub == 0x26) return vec_b5090126_;
  if (sub == 0x27) return vec_b5090127_;
  return {};
}

ebus::DeviceInfo ebus::Device::getDeviceInfo() const {
  DeviceInfo info;
  info.slave_address = slave_;

  if (vec_070400_.size() > 1) {
    info.manufacturer = vec_070400_[1];
    info.manufacturer_name = ebus::manufacturerName(info.manufacturer);
    info.unit_id = ebus::byteToChar(ebus::range(vec_070400_, 2, 5));
    info.software_version = ebus::toString(ebus::range(vec_070400_, 7, 2));
    info.hardware_version = ebus::toString(ebus::range(vec_070400_, 9, 2));
  }

  if (isVaillant() && isVaillantValid()) {
    // Reconstruct the 28-character Vaillant serial number from the 4 B5
    // sub-services
    std::string serial = ebus::byteToChar(ebus::range(vec_b5090124_, 2, 8));
    serial += ebus::byteToChar(ebus::range(vec_b5090125_, 1, 9));
    serial += ebus::byteToChar(ebus::range(vec_b5090126_, 1, 9));
    serial += ebus::byteToChar(ebus::range(vec_b5090127_, 1, 2));

    info.vaillant.serial_number = serial;
    if (serial.length() >= 16) {
      info.vaillant.product_code = serial.substr(6, 10);
    }
  }

  return info;
}

std::vector<uint8_t> ebus::Device::createScanCommand(uint8_t slave) {
  std::vector<uint8_t> command = {slave};
  command.insert(command.end(), VEC_070400.begin(), VEC_070400.end());
  return command;
}

std::vector<std::vector<uint8_t>> ebus::Device::createVendorScanCommands()
    const {
  std::vector<std::vector<uint8_t>> commands;
  if (isVaillant()) {
    if (vec_b5090124_.size() == 0) {
      std::vector<uint8_t> command = {slave_};
      command.insert(command.end(), VEC_b5090124.begin(), VEC_b5090124.end());
      commands.push_back(command);
    }
    if (vec_b5090125_.size() == 0) {
      std::vector<uint8_t> command = {slave_};
      command.insert(command.end(), VEC_b5090125.begin(), VEC_b5090125.end());
      commands.push_back(command);
    }
    if (vec_b5090126_.size() == 0) {
      std::vector<uint8_t> command = {slave_};
      command.insert(command.end(), VEC_b5090126.begin(), VEC_b5090126.end());
      commands.push_back(command);
    }
    if (vec_b5090127_.size() == 0) {
      std::vector<uint8_t> command = {slave_};
      command.insert(command.end(), VEC_b5090127.begin(), VEC_b5090127.end());
      commands.push_back(command);
    }
  }
  return commands;
}

bool ebus::Device::isVaillant() const {
  return (vec_070400_.size() > 1 && vec_070400_[1] == VENDOR_VAILLANT);
}

bool ebus::Device::isVaillantValid() const {
  return (vec_b5090124_.size() > 0 && vec_b5090125_.size() > 0 &&
          vec_b5090126_.size() > 0 && vec_b5090127_.size() > 0);
}
