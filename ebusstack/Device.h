/*
 * Copyright (C) 2012-2025 Roland Jax
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

#include <termios.h>

#include <cstdint>
#include <string>

namespace ebus {

class Device {
 public:
  explicit Device(const std::string &device);
  ~Device();

  void open();
  void close();

  bool isOpen() const;

  void send(const uint8_t byte);
  void recv(uint8_t &byte, const uint8_t sec, const uint16_t nsec);

 private:
  const std::string m_device;

  termios m_oldSettings = {};

  int m_fd = -1;

  bool m_open = false;

  bool isValid();
};

}  // namespace ebus
