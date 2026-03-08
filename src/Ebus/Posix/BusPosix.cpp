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

#if defined(POSIX)
#include "BusPosix.hpp"

#include <iostream>

#include "../Common.hpp"

ebus::BusPosix::BusPosix(const busConfig& config, Request* request)
    : device(config.device),
      simulate(config.simulate),
      request(request),
      fd(-1),
      open(false),
      byteQueue(new Queue<BusEvent>()),
      thread(),
      running(false) {}

ebus::BusPosix::~BusPosix() { stop(); }

void ebus::BusPosix::start() {
  if (open) return;

  struct termios newSettings;
  fd = ::open(device.c_str(), O_RDWR | O_NOCTTY);
  if (fd < 0 || isatty(fd) == 0)
    throw std::runtime_error("Failed to open ebus device: " + device);

  tcgetattr(fd, &oldSettings);
  ::memset(&newSettings, 0, sizeof(newSettings));
  newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
  newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  newSettings.c_iflag |= IGNPAR;
  newSettings.c_oflag &= ~OPOST;
  newSettings.c_cc[VMIN] = 1;
  newSettings.c_cc[VTIME] = 0;

  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSAFLUSH, &newSettings);
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

  open = true;
  running.store(true);
  thread = std::thread(&BusPosix::readerThread, this);
}

void ebus::BusPosix::stop() {
  if (open) {
    running.store(false);
    if (thread.joinable()) thread.join();

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &oldSettings);
    ::close(fd);
    fd = -1;
    open = false;
  }
}

ebus::Queue<ebus::BusEvent>* ebus::BusPosix::getQueue() const {
  return byteQueue.get();
}

void ebus::BusPosix::writeByte(const uint8_t byte) {
  if (simulate) {
    writtenBytes.push_back(byte);
    std::string msg = "<- write: " + ebus::to_string(byte) + "\n";
    std::cout << msg;
    return;
  }
  ensureOpen();
  int ret = ::write(fd, &byte, 1);
  if (ret == -1) throw std::runtime_error("BusPosix: write error");
}

void ebus::BusPosix::setWindow(const uint16_t window) { this->window = window; }

void ebus::BusPosix::setOffset(const uint16_t offset) { this->offset = offset; }

std::string ebus::BusPosix::getSimulatedWrittenBytes() const {
  return ebus::to_string(writtenBytes);
}

void ebus::BusPosix::ensureOpen() const {
  if (!open || fd < 0) throw std::runtime_error("BusPosix: device not open");
}

void ebus::BusPosix::readerThread() {
  bool busRequestFlag = false;
  while (running.load()) {
    uint8_t byte;
    ssize_t n = ::read(fd, &byte, 1);
    if (n == 1) {
      BusEvent event;
      event.byte = byte;
      event.busRequest = busRequestFlag;
      busRequestFlag = false;
      // startBit indicates if the start bit wasn't in the expected window
      // event.startBit = true;

      if (byteQueue) byteQueue->push(event);

      // TODO calculate timing to write address at the right time, currently
      // just write immediately after receiving SYN
      if (byte == sym_syn && request->busRequestPending()) {
        writeByte(request->busRequestAddress());
        busRequestFlag = true;
      }

    } else if (n == 0) {
      // EOF - stop thread
      break;
    } else {
      // read error, optionally break or continue after short sleep
      if (errno == EINTR) continue;
      break;
    }
  }
  running.store(false);
}

#endif
