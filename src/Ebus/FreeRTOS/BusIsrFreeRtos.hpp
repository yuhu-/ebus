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

#include "../Handler.hpp"
#include "../ServiceRunner.hpp"

namespace ebus {

extern ebus::Handler* handler;
extern ebus::ServiceRunner* serviceRunner;

void setupBusIsr(const uart_port_t& uartNum, const int8_t& rxPin,
                 const int8_t& txPin, const uint8_t& timer);

void setBusIsrWindow(const uint16_t& window);
void setBusIsrOffset(const uint16_t& offset);

void processBusIsrEvents();

}  // namespace ebus
