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

struct BusEvent {
  uint8_t byte;
  bool busRequest{false};
  bool startBit{false};
};

class BusPosix {
 public:
  explicit BusPosix(const busConfig& config, Request* request);
  ~BusPosix();

  BusPosix(const BusPosix&) = delete;
  BusPosix& operator=(const BusPosix&) = delete;

  void start();
  void stop();

  Queue<BusEvent>* getQueue() const;

  void writeByte(const uint8_t byte);

  // kept for BusFreeRtos compatibility, but not used in Posix implementation
  void setWindow(const uint16_t window);
  void setOffset(const uint16_t offset);

  // get the last written byte or the full history of written bytes
  uint8_t getLastWrittenByte() const;
  std::string getSimulatedWrittenBytes() const;

 private:
  std::string device_;
  bool simulate_;

  Request* request_ = nullptr;

  int fd_;
  bool open_;
  struct termios oldSettings_{};

  volatile uint16_t window_ = 4300;  // usually between 4300-4456 us
  volatile uint16_t offset_ = 80;    // mainly for context switch and write

  std::unique_ptr<Queue<BusEvent>> byteQueue_;
  std::thread thread_;
  std::atomic<bool> running_;

  // simulated written bytes
  uint8_t lastWrittenByte_ = 0x00;
  std::vector<uint8_t> writtenBytes_;

  void ensureOpen() const;

  // Thread function: read bytes and push them into the queue
  void readerThread();
};

}  // namespace ebus

#endif