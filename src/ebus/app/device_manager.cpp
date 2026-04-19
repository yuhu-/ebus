/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_manager.hpp"

#include <set>

void ebus::DeviceManager::setOwnAddress(uint8_t address) {
  own_address_ = address;
}

void ebus::DeviceManager::update(ByteView master, ByteView slave) {
  std::lock_guard<std::mutex> lock(mutex_);
  // Addresses
  masters_[master[0]]++;
  if (ebus::isSlave(master[1])) slaves_[master[1]]++;

  // Devices
  if (master[1] == ebus::slaveOf(own_address_)) return;
  if (ebus::isSlave(master[1])) {
    devices_[master[1]].update(master, slave);
    identified_devices_.set(master[1]);
  }
}

void ebus::DeviceManager::resetAddresses() {
  std::lock_guard<std::mutex> lock(mutex_);
  masters_.fill(0);
  slaves_.fill(0);
}

std::vector<ebus::DeviceInfo> ebus::DeviceManager::getDeviceInfo() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<DeviceInfo> result;
  for (size_t i = 0; i < 256; ++i) {
    if (identified_devices_.test(i)) {
      result.push_back(devices_[i].getDeviceInfo());
    }
  }
  return result;
}

std::vector<std::pair<uint8_t, uint32_t>> ebus::DeviceManager::getMasters()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::pair<uint8_t, uint32_t>> result;
  for (size_t i = 0; i < masters_.size(); ++i) {
    if (masters_[i] > 0)
      result.push_back({static_cast<uint8_t>(i), masters_[i]});
  }
  return result;
}

std::vector<std::pair<uint8_t, uint32_t>> ebus::DeviceManager::getSlaves()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::pair<uint8_t, uint32_t>> result;
  for (size_t i = 0; i < slaves_.size(); ++i) {
    if (slaves_[i] > 0) result.push_back({static_cast<uint8_t>(i), slaves_[i]});
  }
  return result;
}

uint32_t ebus::DeviceManager::findCounter(uint8_t address) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return masters_[address] + slaves_[address];
}

std::bitset<256> ebus::DeviceManager::getObservedSlaves() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::bitset<256> observed;

  for (size_t i = 0; i < masters_.size(); ++i) {
    if (masters_[i] > 0 && i != own_address_)
      observed.set(ebus::slaveOf(static_cast<uint8_t>(i)));
  }

  for (size_t i = 0; i < slaves_.size(); ++i) {
    if (slaves_[i] > 0 && i != ebus::slaveOf(own_address_))
      observed.set(static_cast<uint8_t>(i));
  }
  return observed;
}

std::vector<ebus::Sequence> ebus::DeviceManager::vendorScanCommands() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Sequence> result;
  for (size_t i = 0; i < 256; ++i) {
    if (identified_devices_.test(i)) {
      const auto commands = devices_[i].createVendorScanCommands();
      if (!commands.empty())
        result.insert(result.end(), commands.begin(), commands.end());
    }
  }
  return result;
}

std::vector<ebus::Sequence> ebus::DeviceManager::createScanCommands(
    const std::vector<std::string>& addresses) const {
  std::set<uint8_t> scan_slaves;
  for (const std::string& address : addresses) {
    const std::vector<uint8_t> bytes = ebus::toVector(address);
    if (bytes.empty()) continue;
    uint8_t first_byte = bytes[0];
    if (ebus::isSlave(first_byte) &&
        (first_byte != ebus::slaveOf(own_address_)))
      scan_slaves.insert(first_byte);
  }
  std::vector<Sequence> result;
  for (const uint8_t slave : scan_slaves)
    result.push_back(Device::createScanCommand(slave));
  return result;
}
