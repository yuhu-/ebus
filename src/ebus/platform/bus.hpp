/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(EBUS_SIMULATION)
#include "simulation/bus_simulation.hpp"
namespace ebus::detail::platform {
class Bus : public BusSimulation {
 public:
  using BusSimulation::BusSimulation;
};
}  // namespace ebus::detail::platform
#elif defined(ESP_PLATFORM) && !defined(EBUS_SIMULATION)
#include "esp/bus_esp.hpp"
namespace ebus::detail::platform {
class Bus : public BusEsp {
 public:
  using BusEsp::BusEsp;
};
}  // namespace ebus::detail::platform
#elif defined(POSIX) && !defined(EBUS_SIMULATION)
#include "posix/bus_posix.hpp"
namespace ebus::detail::platform {
class Bus : public BusPosix {
 public:
  using BusPosix::BusPosix;
};
}  // namespace ebus::detail::platform
#endif
