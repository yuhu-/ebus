/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "models/device.hpp"

#include <algorithm>
#include <ebus/data_types.hpp>
#include <ebus/device.hpp>
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

void ebus::Device::update(ByteView master_view, ByteView slave_view) {
  slave_ = master_view[1];
  if (ebus::matches(master_view, VEC_070400, 2))
    vec_070400_.assign(slave_view);
  else if (ebus::matches(master_view, VEC_b5090124, 2))
    vec_b5090124_.assign(slave_view);
  else if (ebus::matches(master_view, VEC_b5090125, 2))
    vec_b5090125_.assign(slave_view);
  else if (ebus::matches(master_view, VEC_b5090126, 2))
    vec_b5090126_.assign(slave_view);
  else if (ebus::matches(master_view, VEC_b5090127, 2))
    vec_b5090127_.assign(slave_view);
}

std::vector<uint8_t> ebus::Device::getIdentificationData() const {
  return vec_070400_.toVector();
}

std::vector<uint8_t> ebus::Device::getVendorData(uint8_t sub) const {
  if (sub == 0x24) return vec_b5090124_.toVector();
  if (sub == 0x25) return vec_b5090125_.toVector();
  if (sub == 0x26) return vec_b5090126_.toVector();
  if (sub == 0x27) return vec_b5090127_.toVector();
  return {};
}

ebus::DeviceInfo ebus::Device::getDeviceInfo() const {
  DeviceInfo info;
  info.slave_address = slave_;

  if (vec_070400_.size() > 1) {
    info.manufacturer = vec_070400_[1];
    info.manufacturer_name = ebus::manufacturerName(info.manufacturer);
    info.unit_id = ebus::byteToChar(ebus::range(vec_070400_, 2, 5));
    info.software_version =
        ebus::toString(ebus::ByteView(vec_070400_.data() + 7, 2));
    info.hardware_version =
        ebus::toString(ebus::ByteView(vec_070400_.data() + 9, 2));
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

ebus::Sequence ebus::Device::createScanCommand(uint8_t slave) {
  Sequence sequence;
  sequence.pushBack(slave, false);
  for (uint8_t b : VEC_070400) sequence.pushBack(b, false);
  return sequence;
}

std::vector<ebus::Sequence> ebus::Device::createVendorScanCommands() const {
  std::vector<Sequence> commands;
  if (isVaillant()) {
    if (vec_b5090124_.size() == 0) {
      Sequence command;
      command.pushBack(slave_);
      for (uint8_t b : VEC_b5090124) command.pushBack(b, false);
      commands.push_back(command);
    }
    if (vec_b5090125_.size() == 0) {
      Sequence command;
      command.pushBack(slave_, false);
      for (uint8_t b : VEC_b5090125) command.pushBack(b, false);
      commands.push_back(command);
    }
    if (vec_b5090126_.size() == 0) {
      Sequence command;
      command.pushBack(slave_, false);
      for (uint8_t b : VEC_b5090126) command.pushBack(b, false);
      commands.push_back(command);
    }
    if (vec_b5090127_.size() == 0) {
      Sequence command;
      command.pushBack(slave_, false);
      for (uint8_t b : VEC_b5090127) command.pushBack(b, false);
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

static constexpr const char* kManufacturerTable[256] = {
    nullptr,         nullptr,         nullptr,      "Junkers",   "Bosch",
    "Buderus",       "Dungs",         nullptr,      "Siemens",   nullptr,
    nullptr,         "Danfoss",       nullptr,      nullptr,     nullptr,
    "FH Ostfalia",   "TEM",           "Lamberti",   nullptr,     nullptr,
    "CEB",           "Landis-Staefa", "FERRO",      "MONDIAL",   "Wikon",
    "Wolf",          "Elco",          nullptr,      nullptr,     nullptr,
    nullptr,         "RAWE",          nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Satronic",  nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     "ENCON",
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Paradigma",     "Ochsner",       nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Kromschroeder", nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "Eberle",        nullptr,      nullptr,     nullptr,
    nullptr,         "EBV",           nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Graesslin", nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Gira",      nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     "ebm-papst",
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "SIG",       nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Theben",        nullptr,         "Thermowatt", nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Vaillant",      nullptr,         "Protherm",   nullptr,     nullptr,
    nullptr,         "Saunier Duval", nullptr,      nullptr,     nullptr,
    nullptr,         "Toby",          nullptr,      nullptr,     nullptr,
    nullptr,         "Weishaupt",     nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         "Honeywell",  nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "Wolf",          nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         "danman.eu",  nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "ebusd.eu",      nullptr,      nullptr};

const char* ebus::manufacturerName(uint8_t id) {
  const char* name = kManufacturerTable[id];
  return name ? name : "Unknown";
}
