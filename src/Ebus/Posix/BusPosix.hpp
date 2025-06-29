/*
 * Copyright (C) 2025 Roland Jax
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

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

// Example
// ebus::BusPosix bus("/dev/ttyUSB0");
// bus.open();
// bus.writeByte(0x42);
// if (bus.available()) {
//     uint8_t b = bus.readByte();
//     // process b
// }
// bus.close();

namespace ebus {

class BusPosix {
 public:
  explicit BusPosix(const std::string& device)
      : m_device(device), m_fd(-1), m_open(false) {}

  ~BusPosix() { close(); }

  void open() {
    if (m_open) return;

    struct termios newSettings;
    m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY);
    if (m_fd < 0 || isatty(m_fd) == 0)
      throw std::runtime_error("Failed to open ebus device: " + m_device);

    tcgetattr(m_fd, &m_oldSettings);
    ::memset(&newSettings, 0, sizeof(newSettings));
    newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
    newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newSettings.c_iflag |= IGNPAR;
    newSettings.c_oflag &= ~OPOST;
    newSettings.c_cc[VMIN] = 1;
    newSettings.c_cc[VTIME] = 0;

    tcflush(m_fd, TCIFLUSH);
    tcsetattr(m_fd, TCSAFLUSH, &newSettings);
    fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

    m_open = true;
  }

  void close() {
    if (m_open) {
      tcflush(m_fd, TCIOFLUSH);
      tcsetattr(m_fd, TCSANOW, &m_oldSettings);
      ::close(m_fd);
      m_fd = -1;
      m_open = false;
    }
  }

  bool isOpen() const { return m_open; }

  // Write a single byte to the bus
  void writeByte(const uint8_t byte) {
    ensureOpen();
    int ret = ::write(m_fd, &byte, 1);
    if (ret == -1) throw std::runtime_error("BusPosix: write error");
  }

  // Read a single byte from the bus (blocking)
  uint8_t readByte() {
    ensureOpen();
    uint8_t byte;
    ssize_t nbytes = ::read(m_fd, &byte, 1);
    if (nbytes < 0) throw std::runtime_error("BusPosix: read error");
    if (nbytes == 0) throw std::runtime_error("BusPosix: EOF on read");
    return byte;
  }

  // Returns the number of bytes available to read (non-blocking)
  size_t available() const {
    ensureOpen();
    int bytes = 0;
    if (ioctl(m_fd, FIONREAD, &bytes) == -1)
      throw std::runtime_error("BusPosix: ioctl FIONREAD failed");
    return static_cast<size_t>(bytes);
  }

 private:
  std::string m_device;
  int m_fd;
  bool m_open;
  struct termios m_oldSettings{};

  void ensureOpen() const {
    if (!m_open || m_fd < 0)
      throw std::runtime_error("BusPosix: device not open");
  }
};

}  // namespace ebus
