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

#include <HardwareSerial.h>

#include "../Bus.hpp"

namespace ebus {

class BusFreeRtos {
 public:
  explicit BusFreeRtos(HardwareSerial& serial) : serial(serial) {}

  void writeByte(const uint8_t byte) { serial.write(byte); }

  uint8_t readByte() { return serial.read(); }

  size_t available() const { return serial.available(); }

 private:
  HardwareSerial& serial;
};

}  // namespace ebus
