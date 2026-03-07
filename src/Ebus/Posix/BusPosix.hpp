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
#include "../Request.hpp"

namespace ebus {

struct busConfig {
  const char* device;
  bool simulate;
};

class BusPosix {
 public:
  explicit BusPosix(const busConfig& config, Request* request);
  ~BusPosix();

  void start();
  void stop();

  Queue<uint8_t>* getQueue() const;

  void writeByte(const uint8_t byte);

  // kept for BusFreeRtos compatibility, but not used in Posix implementation
  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);

  // Posix specific
  uint8_t readByte();
  size_t available() const;

  // Get all simulated written bytes as a hex string
  std::string getSimulatedWrittenBytes() const;

 private:
  BusPosix(const BusPosix&) = delete;
  BusPosix& operator=(const BusPosix&) = delete;

  std::string m_device;
  bool m_simulate;

  Request* m_request = nullptr;

  int m_fd;
  bool m_open;
  struct termios m_oldSettings{};

  volatile uint16_t m_busIsrWindow = 4300;  // usually between 4300-4456 us
  volatile uint16_t m_busIsrOffset = 80;  // mainly for context switch and write

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