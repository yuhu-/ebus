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

#if defined(ESP32)
#include "FreeRTOS/QueueFreeRtos.hpp"
namespace ebus {
template <typename T>
using Queue = QueueFreeRtos<T>;
}
#else
#include "Posix/QueuePosix.hpp"
namespace ebus {
template <typename T>
using Queue = QueuePosix<T>;
}
#endif
