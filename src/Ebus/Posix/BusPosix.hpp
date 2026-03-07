/*
 * Copyright (C) 2025-2026 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#pragma once

#if defined(POSIX)
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../Queue.hpp"

namespace ebus {

typedef struct {
  const char* device;
  bool simulate;
} bus_config_t;

class BusPosix {
 public:
  explicit BusPosix(bus_config_t& config);
  ~BusPosix();

  void start();
  void stop();

  // Write a single byte to the bus
  void writeByte(const uint8_t byte);

  // Read a single byte from the bus (blocking) - kept for API compatibility
  uint8_t readByte();

  // Returns the number of bytes available to read (non-blocking)
  size_t available() const;

  // Access to the internal byte queue (matches BusFreeRtos::getQueue)
  Queue<uint8_t>* getQueue() const;

  // Get all simulated written bytes as a hex string
  std::string getSimulatedWrittenBytes() const;

 private:
  BusPosix(const BusPosix&) = delete;
  BusPosix& operator=(const BusPosix&) = delete;

  std::string m_device;
  bool m_simulate;

  int m_fd;
  bool m_open;
  struct termios m_oldSettings{};

  std::unique_ptr<Queue<uint8_t>> m_byteQueue;
  std::thread m_thread;
  std::atomic<bool> m_running;

  // simulated written bytes
  std::vector<uint8_t> m_writtenBytes;

  void ensureOpen() const;

  // Thread function: read bytes and push them into the queue
  void readerThread();
};

}  // namespace ebus

#endif