/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_manager.hpp"

#include "core/bus_monitor.hpp"

namespace ebus::detail {

DeviceManager::DeviceManager(BusMonitor* monitor) : monitor_(monitor) {
  address_map_.fill(-1);
}

void DeviceManager::setOwnAddress(uint8_t address) { own_address_ = address; }

void DeviceManager::update(ByteView master_view, ByteView slave_view) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  uint8_t m_addr = master_view[0];
  uint8_t s_addr = master_view[1];

  if (monitor_) {
    monitor_->updateDevice([&](metrics::DeviceMetrics& d) {
      auto is_new = [&](uint8_t addr) {
        uint8_t sa = ebus::isSlave(addr) ? addr : ebus::slaveOf(addr);
        return !masters_.test(masterOf(sa)) && !slaves_.test(sa);
      };

      if (is_new(m_addr) && !identified_devices_.test(ebus::slaveOf(m_addr))) {
        d.unknown_devices++;
      }
      masters_.set(m_addr);

      if (ebus::isSlave(s_addr)) {
        if (is_new(s_addr) && !identified_devices_.test(s_addr)) {
          d.unknown_devices++;
        }
        slaves_.set(s_addr);
      }
    });
  }

  // Device Inventory & Frequency Tracking
  uint8_t target = master_view[1];

  auto updateEntry = [&](uint8_t slave_addr, ByteView m_view, ByteView s_view) {
    if (slave_addr == ebus::slaveOf(own_address_)) return;

    int16_t idx = address_map_[slave_addr];
    if (idx == -1) {
      if (pool_usage_ >= max_devices_) return;

      idx = static_cast<int16_t>(pool_usage_++);
      address_map_[slave_addr] = idx;
      identified_devices_.set(slave_addr);

      if (monitor_) {
        monitor_->updateDevice([](auto& d) {
          if (d.unknown_devices > 0) d.unknown_devices--;
        });
        monitor_->updateDevice([this](auto& d) {
          d.identified_devices = static_cast<uint32_t>(pool_usage_);
        });
      }
    }
    device_pool_[idx].update(slave_addr, m_view, s_view);
  };

  // 1. Track Source Activity (Master)
  if (ebus::isMaster(m_addr))
    updateEntry(ebus::slaveOf(m_addr), master_view, {});

  // 2. Track Target Activity (Master or Slave)
  if (ebus::isMaster(target))
    updateEntry(ebus::slaveOf(target), master_view, {});
  else if (ebus::isSlave(target))
    updateEntry(target, master_view, slave_view);
}

uint16_t DeviceManager::findNextObservedSlave(uint8_t start) const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  const uint8_t own_slave = ebus::slaveOf(own_address_);

  for (uint16_t s = start; s < 256; ++s) {
    const uint8_t addr = static_cast<uint8_t>(s);
    if (addr == own_slave) continue;

    // Check if 'addr' is an observed slave address
    if (slaves_.test(addr)) return s;

    // Check if 'addr' is a master address whose slave counterpart is observed
    // This ensures we scan the slave side of any observed master.
    if (ebus::isMaster(addr) && masters_.test(addr)) return ebus::slaveOf(addr);
  }
  return 256;
}

bool DeviceManager::getNextPendingVendorCommandForDevice(
    uint8_t device_addr, uint16_t& cursor, Sequence& out_cmd) const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  const int16_t idx = address_map_[device_addr];
  if (idx != -1) {
    // Ensure the cursor is within valid bounds for the device's vendor commands
    if (cursor < 4) {  // Assuming max 4 vendor commands for now (Vaillant)
      return device_pool_[idx].getNextPendingVendorCommand(cursor, out_cmd);
    }
  }
  cursor = 4;    // Mark as exhausted
  return false;  // No more commands
}

uint16_t DeviceManager::findNextPendingVendorCommand(uint16_t start_addr,
                                                     Sequence& out_cmd) const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  for (uint16_t addr = start_addr; addr < 256; ++addr) {
    int16_t idx = address_map_[addr];
    if (idx != -1 && device_pool_[idx].getFirstPendingVendorCommand(out_cmd)) {
      return addr;
    }
  }
  return 256;
}

bool DeviceManager::isIdentified(uint8_t addr) const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  int16_t idx = address_map_[addr];
  return idx != -1 && device_pool_[idx].isIdentified();
}

bool DeviceManager::needsDeepScan(uint8_t addr) const {
  Sequence dummy;
  return isIdentified(addr) && findNextPendingVendorCommand(addr, dummy) < 256;
}

void DeviceManager::getObservedSlaves(std::bitset<256>& observed) const {
  observed.reset();  // Clear any previous state
  for (size_t i = 0; i < 256; ++i) {
    if (masters_.test(i) && i != own_address_) {
      observed.set(ebus::slaveOf(static_cast<uint8_t>(i)));
    }
    if (slaves_.test(i) && i != ebus::slaveOf(own_address_)) {
      observed.set(static_cast<uint8_t>(i));
    }
  }
}

void DeviceManager::fetchDevices(
    const std::function<void(const DeviceInfo&)>& callback) const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  if (callback) {
    for (size_t i = 0; i < 256; ++i) {
      int16_t idx = address_map_[i];
      if (idx != -1) callback(device_pool_[idx].getDevice());
    }
  }
}

DeviceManagerStatus DeviceManager::fetchStatus() const {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  DeviceManagerStatus s;
  s.identified_count = identified_devices_.count();
  s.device_capacity = max_devices_;
  if (monitor_) {
    monitor_->fetchMetrics(
        [&](const Metrics& m) { s.unknown_count = m.devices.unknown_devices; });
  }
  return s;
}

}  // namespace ebus::detail