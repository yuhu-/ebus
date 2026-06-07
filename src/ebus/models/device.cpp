/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "models/device.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <ebus/data_types.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/device.hpp>
#include <ebus/utils.hpp>

namespace ebus::detail {

static constexpr uint8_t VENDOR_VAILLANT = 0xb5;

// Identification (Service 07h 04h)
static constexpr std::array<uint8_t, 3> VEC_070400 = {0x07, 0x04, 0x00};

// Vaillant identification (Service B5h 09h 24h-27h)
static constexpr std::array<uint8_t, 4> VEC_b5090124 = {0xb5, 0x09, 0x01, 0x24};
static constexpr std::array<uint8_t, 4> VEC_b5090125 = {0xb5, 0x09, 0x01, 0x25};
static constexpr std::array<uint8_t, 4> VEC_b5090126 = {0xb5, 0x09, 0x01, 0x26};
static constexpr std::array<uint8_t, 4> VEC_b5090127 = {0xb5, 0x09, 0x01, 0x27};

ebus::Sequence Device::createScanCommand(uint8_t slave) {
  Sequence sequence;
  sequence.pushBack(slave, false);
  for (uint8_t b : VEC_070400) sequence.pushBack(b, false);
  return sequence;
}

void Device::update(uint8_t slave_addr, ByteView master_view,
                    ByteView slave_view) {
  slave_ = slave_addr;
  message_count_++;

  if (slave_view.empty()) return;

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

void Device::createVendorScanCommands(
    const std::function<void(const Sequence&)>& callback) const {
  if (!callback) return;

  if (isVaillant()) {
    const ModelSequence* storage_fields[] = {&vec_b5090124_, &vec_b5090125_,
                                             &vec_b5090126_, &vec_b5090127_};
    const std::array<uint8_t, 4>* command_prefixes[] = {
        &VEC_b5090124, &VEC_b5090125, &VEC_b5090126, &VEC_b5090127};

    for (size_t i = 0; i < 4; ++i) {
      if (storage_fields[i]->empty()) {
        Sequence command;
        command.pushBack(slave_, false);
        for (uint8_t b : *command_prefixes[i]) command.pushBack(b, false);
        callback(command);
      }
    }
  }
  // To support a new vendor, add a similar block here or use a manufacturer
  // lookup table.
}

uint8_t Device::getSlave() const { return slave_; }

std::vector<uint8_t> Device::getIdentificationData() const {
  return vec_070400_.toVector();
}

std::vector<uint8_t> Device::getVendorData(uint8_t sub) const {
  if (sub == 0x24) return vec_b5090124_.toVector();
  if (sub == 0x25) return vec_b5090125_.toVector();
  if (sub == 0x26) return vec_b5090126_.toVector();
  if (sub == 0x27) return vec_b5090127_.toVector();
  return {};
}

ebus::DeviceInfo Device::getDeviceInfo() const {
  DeviceInfo info;
  info.slave_address = slave_;
  info.frequency = message_count_;

  if (vec_070400_.size() >= 11) {
    info.manufacturer = vec_070400_[1];
    info.manufacturer_name = manufacturerName(info.manufacturer);
    info.unit_id = ebus::range(vec_070400_, 2, 5);
    info.software_version = ebus::range(vec_070400_, 7, 2);
    info.hardware_version = ebus::range(vec_070400_, 9, 2);
  }

  if (isVaillant() && isVaillantValid()) {
    auto& sn = info.vaillant.serial_number;
    sn.clear();
    auto appendSegment = [&](ByteView src, size_t offset, size_t len) {
      if (src.size() >= offset + len && sn.size_bytes + len <= 28) {
        std::memcpy(sn.buffer + sn.size_bytes, src.data() + offset, len);
        sn.size_bytes += static_cast<uint8_t>(len);
      }
    };

    appendSegment(vec_b5090124_, 2, 8);
    appendSegment(vec_b5090125_, 1, 9);
    appendSegment(vec_b5090126_, 1, 9);
    appendSegment(vec_b5090127_, 1, 2);

    if (sn.size_bytes >= 16) {
      info.vaillant.product_code = ByteView(sn.buffer + 6, 10);
    }
  }

  return info;
}

bool Device::isVaillant() const {
  return (vec_070400_.size() > 1 && vec_070400_[1] == VENDOR_VAILLANT);
}

bool Device::isVaillantValid() const {
  return (vec_b5090124_.size() > 0 && vec_b5090125_.size() > 0 &&
          vec_b5090126_.size() > 0 && vec_b5090127_.size() > 0);
}

}  // namespace ebus::detail

namespace ebus {

void DeviceInfo::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeHexField("slave_address", ByteView(&slave_address, 1));
  writer.writeHexField("manufacturer", ByteView(&manufacturer, 1));
  writer.writeField("manufacturer_name",
                    manufacturer_name ? manufacturer_name : "Unknown");

  auto writeAscii = [&](std::string_view key, ByteView val) {
    writer.writeField(
        key, std::string_view(reinterpret_cast<const char*>(val.data()),
                              val.size()));
  };

  writeAscii("unit_id", unit_id);
  writer.writeHexField("software_version", software_version);
  writer.writeHexField("hardware_version", hardware_version);

  if (vaillant.serial_number.size() > 0) {
    {
      auto vScope = writer.objectScope("vaillant");
      writeAscii("serial_number", vaillant.serial_number);
      writeAscii("product_code", vaillant.product_code);
    }
  }

  writer.writeField("frequency", frequency);
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

const char* manufacturerName(uint8_t id) {
  const char* name = kManufacturerTable[id];
  return name ? name : "Unknown";
}

}  // namespace ebus
