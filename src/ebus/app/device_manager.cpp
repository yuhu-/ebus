/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_manager.hpp"

#include <set>

void ebus::DeviceManager::setOwnAddress(uint8_t address) {
  own_address_ = address;
}

void ebus::DeviceManager::update(const std::vector<uint8_t>& master,
                                 const std::vector<uint8_t>& slave) {
  std::lock_guard<std::mutex> lock(mutex_);
  // Addresses
  masters_[master[0]]++;
  if (ebus::isSlave(master[1])) slaves_[master[1]]++;

  // Devices
  if (master[1] == ebus::slaveOf(own_address_)) return;
  if (ebus::isSlave(master[1])) devices_[master[1]].update(master, slave);
}

void ebus::DeviceManager::resetAddresses() {
  std::lock_guard<std::mutex> lock(mutex_);
  masters_.clear();
  slaves_.clear();
}

std::vector<ebus::DeviceInfo> ebus::DeviceManager::getDeviceInfo() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<DeviceInfo> result;
  result.reserve(devices_.size());
  for (const auto& device : devices_) {
    result.push_back(device.second.getDeviceInfo());
  }
  return result;
}

std::map<uint8_t, uint32_t> ebus::DeviceManager::getMasters() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return masters_;
}

std::map<uint8_t, uint32_t> ebus::DeviceManager::getSlaves() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return slaves_;
}

std::set<uint8_t> ebus::DeviceManager::getObservedSlaves() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::set<uint8_t> slaves;

  for (const auto& master : masters_) {
    if (master.first != own_address_)
      slaves.insert(ebus::slaveOf(master.first));
  }

  for (const auto& slave : slaves_) {
    if (slave.first != ebus::slaveOf(own_address_)) slaves.insert(slave.first);
  }
  return slaves;
}

const std::vector<std::vector<uint8_t>>
ebus::DeviceManager::vendorScanCommands() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::vector<uint8_t>> result;
  for (const auto& device : devices_) {
    const auto commands = device.second.createVendorScanCommands();
    if (!commands.empty())
      result.insert(result.end(), commands.begin(), commands.end());
  }
  return result;
}

const std::vector<std::vector<uint8_t>> ebus::DeviceManager::createScanCommands(
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
  std::vector<std::vector<uint8_t>> result;
  for (const uint8_t slave : scan_slaves)
    result.push_back(Device::createScanCommand(slave));
  return result;
}
