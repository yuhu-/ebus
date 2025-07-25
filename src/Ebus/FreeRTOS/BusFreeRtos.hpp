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

#include "driver/uart.h"

namespace ebus {

class BusFreeRtos {
 public:
  explicit BusFreeRtos(uart_port_t uartNum) : uartNum(uartNum) {}

  void writeByte(const uint8_t byte) {
    uart_write_bytes(uartNum, static_cast<const void*>(&byte), 1);
  }

 private:
  uart_port_t uartNum;
};

}  // namespace ebus
