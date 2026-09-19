/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/device_manager.hpp"

#include <cstring>

#include "core/bus_monitor.hpp"

namespace ebus::detail {

DeviceManager::DeviceManager(BusMonitor* bus_monitor)
    : bus_monitor_(bus_monitor) {
  address_map_.fill(-1);
}

void DeviceManager::setOwnAddress(uint8_t address) { own_address_ = address; }

void DeviceManager::update(ByteView master_view, ByteView slave_view) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  uint8_t m_addr = master_view[0];
  uint8_t s_addr = master_view[1];

  // Observation bitsets (kept for getObservedSlaves and the scanner).
  masters_.set(m_addr);
  if (ebus::isSlave(s_addr)) slaves_.set(s_addr);

  // Device Inventory & Frequency Tracking
  uint8_t target = master_view[1];

  auto updateEntry = [&](uint8_t slave_addr, ByteView m_view, ByteView s_view) {
    if (slave_addr == ebus::slaveOf(own_address_)) return;

    int16_t idx = address_map_[slave_addr];
    if (idx == -1) {
      // Allocate pool slots only for telegrams carrying slave response data.
      // Master-only observations (broadcasts, unanswered requests, our own
      // transmissions) stay in the observed bitsets: per Spec §4.1.1 a master
      // HAS a slave address, but the slave side only exists if it actually
      // communicates. Allocating for mere traffic creates phantom devices.
      if (s_view.empty()) return;
      if (pool_usage_ >= max_devices_) return;

      idx = static_cast<int16_t>(pool_usage_++);
      address_map_[slave_addr] = idx;
    }
    device_pool_[idx].update(slave_addr, m_view, s_view);
    if (device_pool_[idx].isIdentified()) identified_devices_.set(slave_addr);
  };

  // 1. Track Source Activity (Master)
  if (ebus::isMaster(m_addr))
    updateEntry(ebus::slaveOf(m_addr), master_view, {});

  // 2. Track Target Activity (Master or Slave)
  if (ebus::isMaster(target))
    updateEntry(ebus::slaveOf(target), master_view, {});
  else if (ebus::isSlave(target))
    updateEntry(target, master_view, slave_view);

  updateDeviceMetrics();
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

void DeviceManager::pruneUnidentified(uint8_t addr) {
  platform::LockGuard<platform::Mutex> lock(mutex_);
  int16_t idx = address_map_[addr];
  if (idx == -1) return;
  if (device_pool_[static_cast<size_t>(idx)].isIdentified()) return;

  // Swap-with-last compaction to keep the pool dense.
  size_t last = --pool_usage_;
  if (static_cast<size_t>(idx) != last) {
    device_pool_[static_cast<size_t>(idx)] = device_pool_[last];
    address_map_[device_pool_[static_cast<size_t>(idx)].getSlave()] =
        static_cast<int16_t>(idx);
  }
  address_map_[addr] = -1;
  identified_devices_.reset(addr);
  updateDeviceMetrics();
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

bool DeviceManager::writeDeviceJson(uint8_t slave_addr, char* out,
                                    size_t capacity, size_t& used) const {
  struct Sink {
    char* buf;
    size_t cap;
    size_t len = 0;
    bool overflow = false;
  };
  Sink sink{out, capacity};
  {
    platform::LockGuard<platform::Mutex> lock(mutex_);
    if (out == nullptr || capacity == 0) return false;
    const int16_t idx = address_map_[slave_addr];
    if (idx == -1) return false;
    detail::JsonWriter writer([&sink](std::string_view s) {
      if (sink.overflow) return;
      if (s.size() > sink.cap - sink.len) {
        sink.overflow = true;
        return;
      }
      std::memcpy(sink.buf + sink.len, s.data(), s.size());
      sink.len += s.size();
    });
    writer.writeValue(device_pool_[static_cast<size_t>(idx)].getDevice());
  }
  if (sink.overflow) return false;
  used = sink.len;
  return true;
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
  if (bus_monitor_) {
    bus_monitor_->fetchMetrics(
        [&](const Metrics& m) { s.unknown_count = m.devices.unknown_devices; });
  }
  return s;
}

void DeviceManager::updateDeviceMetrics() {
  if (!bus_monitor_) return;
  bus_monitor_->updateDevice([&](metrics::DeviceMetrics& d) {
    // Count distinct observed devices (slave side), excluding ourselves:
    // a device is observed via its slave address or via its master's.
    uint32_t observed = 0;
    uint32_t identified = 0;
    const uint8_t own_slave = ebus::slaveOf(own_address_);
    for (uint16_t s = 0; s < 256; ++s) {
      const auto slave_addr = static_cast<uint8_t>(s);
      if (slave_addr == own_slave) continue;
      bool seen = slaves_.test(s);
      if (!seen) {
        const uint8_t master_addr = ebus::masterOf(slave_addr);
        seen = (master_addr != slave_addr) && masters_.test(master_addr);
      }
      if (!seen) continue;
      ++observed;
      if (identified_devices_.test(s)) ++identified;
    }
    d.unknown_devices = observed - identified;
    d.identified_devices = identified;
  });
}

}  // namespace ebus::detail
