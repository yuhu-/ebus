/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_manager.hpp"

#include "core/bus_monitor.hpp"

namespace ebus::detail {

DeviceManager::DeviceManager(BusMonitor* monitor) : monitor_(monitor) {}

void DeviceManager::setOwnAddress(uint8_t address) { own_address_ = address; }

void DeviceManager::update(ByteView master_view, ByteView slave_view) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint8_t m_addr = master_view[0];
  uint8_t s_addr = master_view[1];

  if (monitor_) {
    monitor_->updateDevice([&](metrics::DeviceMetrics& d) {
      auto is_new = [&](uint8_t addr) {
        uint8_t sa = ebus::isSlave(addr) ? addr : ebus::slaveOf(addr);
        return d.masters[masterOf(sa)] == 0 && d.slaves[sa] == 0;
      };

      if (is_new(m_addr) && !identified_devices_.test(ebus::slaveOf(m_addr))) {
        d.unknown_devices++;
      }
      d.masters[m_addr]++;

      if (ebus::isSlave(s_addr)) {
        if (is_new(s_addr) && !identified_devices_.test(s_addr)) {
          d.unknown_devices++;
        }
        d.slaves[s_addr]++;
      }
    });
  }

  // Devices
  if (master_view[1] == ebus::slaveOf(own_address_)) return;
  if (ebus::isSlave(master_view[1])) {
    if (!identified_devices_.test(master_view[1])) {
      if (monitor_) {
        monitor_->updateDevice([](auto& d) {
          if (d.unknown_devices > 0) d.unknown_devices--;
        });
      }
      identified_devices_.set(master_view[1]);
    }
    devices_[master_view[1]].update(master_view, slave_view);
  }
}

std::vector<ebus::DeviceInfo> DeviceManager::getDeviceInfo() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<DeviceInfo> result;
  result.reserve(devices_.size());
  for (const auto& [addr, dev] : devices_) {
    if (identified_devices_.test(addr)) {
      result.push_back(dev.getDeviceInfo());
    }
  }
  return result;
}

void DeviceManager::getObservedSlaves(std::bitset<256>& observed) const {
  observed.reset();  // Clear any previous state
  if (monitor_) {
    auto m = monitor_->getMetrics().devices;

    for (size_t i = 0; i < m.masters.size(); ++i) {
      if (m.masters[i] > 0 && i != own_address_)
        observed.set(ebus::slaveOf(static_cast<uint8_t>(i)));
    }

    for (size_t i = 0; i < m.slaves.size(); ++i) {
      if (m.slaves[i] > 0 && i != ebus::slaveOf(own_address_))
        observed.set(static_cast<uint8_t>(i));
    }
  }
}

std::vector<ebus::Sequence> DeviceManager::vendorScanCommands() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Sequence> result;
  for (const auto& [addr, dev] : devices_) {
    if (identified_devices_.test(addr)) {
      const auto commands = dev.createVendorScanCommands();
      if (!commands.empty())
        result.insert(result.end(), commands.begin(), commands.end());
    }
  }
  return result;
}

std::vector<ebus::Sequence> DeviceManager::createScanCommands(
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

DeviceManagerStatus DeviceManager::getStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  DeviceManagerStatus s;
  s.identified_count = identified_devices_.count();
  if (monitor_) {
    s.unknown_count = monitor_->getMetrics().devices.unknown_devices;
  }
  return s;
}

}  // namespace ebus::detail