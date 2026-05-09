/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/detail/config_validator.hpp>

using namespace ebus;
using namespace ebus::detail;

TEST_CASE("ConfigValidator: Validation Logic", "[detail][config]") {
  EbusConfig config;
  config.runtime.address = 0x31;
  config.bus.device = "/dev/ttyUSB0";

  SECTION("Valid configuration") {
    REQUIRE(ConfigValidator::validate(config) == true);
  }

  SECTION("Invalid addressing") {
    config.runtime.address = 0x02; // Not a master address
    REQUIRE(ConfigValidator::validate(config) == false);
    
    config.runtime.address = 0x31;
    config.runtime.lock_counter = 30; // Exceeds lock_counter_max (25)
    REQUIRE(ConfigValidator::validate(config) == false);
  }

  SECTION("Invalid bus timings") {
    config.runtime.bus.window_us = 2000; // Too low
    REQUIRE(ConfigValidator::validate(config) == false);
    
    config.runtime.bus.window_us = 4300;
    config.runtime.bus.offset_us = 600; // Too high
    REQUIRE(ConfigValidator::validate(config) == false);
  }

  SECTION("Timeout constraints") {
    config.runtime.scheduler.fsm_timeout_ms = 1000;
    config.runtime.scheduler.total_timeout_ms = 500; // Total must be > FSM
    REQUIRE(ConfigValidator::validate(config) == false);
  }
}

TEST_CASE("ConfigValidator: Hardware Restart Detection", "[detail][config]") {
  EbusConfig old_cfg, new_cfg;
  old_cfg.bus.device = "/dev/ttyUSB0";
  new_cfg.bus.device = "/dev/ttyUSB0";

  SECTION("Runtime change (no restart)") {
    new_cfg.runtime.address = 0x10;
    REQUIRE(ConfigValidator::requiresHardwareRestart(old_cfg, new_cfg) == false);
  }

  SECTION("Hardware change (restart required)") {
    new_cfg.bus.device = "/dev/ttyUSB1";
    REQUIRE(ConfigValidator::requiresHardwareRestart(old_cfg, new_cfg) == true);
  }
}
