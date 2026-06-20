/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/detail/config_validator.hpp>

using namespace ebus::detail;

TEST_CASE("ConfigValidator: Validation Logic", "[detail][config]") {
  ebus::EbusConfig config;
  config.runtime.address = 0x31;

  SECTION("Valid configuration") {
    REQUIRE(ConfigValidator::validate(config) == true);
  }

  SECTION("Invalid addressing") {
    config.runtime.address = 0x02;  // Not a master address
    REQUIRE(ConfigValidator::validate(config) == false);

    config.runtime.address = 0x31;
    config.runtime.lock_counter = 30;  // Exceeds lock_counter_max (25)
    REQUIRE(ConfigValidator::validate(config) == false);
  }

  SECTION("Invalid bus timings") {
    config.runtime.bus.window_us = 2000;  // Too low
    REQUIRE(ConfigValidator::validate(config) == false);

    config.runtime.bus.window_us = 4300;
    config.runtime.bus.offset_us = 600;  // Too high
    REQUIRE(ConfigValidator::validate(config) == false);
  }

  SECTION("Timeout constraints") {
    config.runtime.scheduler.fsm_timeout_ms = 1000;
    config.runtime.scheduler.total_timeout_ms = 500;  // Total must be > FSM
    REQUIRE(ConfigValidator::validate(config) == false);
  }

  SECTION("Network server validation") {
    config.runtime.network.enable_server = true;
    config.runtime.network.port_regular = 3333;
    config.runtime.network.port_readonly = 3334;
    config.runtime.network.port_enhanced = 3335;
    REQUIRE(ConfigValidator::validate(config) == true);

    // Invalid port 0
    config.runtime.network.port_regular = 0;
    REQUIRE(ConfigValidator::validate(config) == false);
    config.runtime.network.port_regular = 3333;

    // Duplicate ports
    config.runtime.network.port_readonly = 3333;
    REQUIRE(ConfigValidator::validate(config) == false);
  }
}
