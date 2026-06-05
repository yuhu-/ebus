/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <string>

#include "utils/logger.hpp"

using namespace ebus::detail;

TEST_CASE("Logger: Filtering and Sinks", "[utils][logger]") {
  auto& logger = Logger::getInstance();
  std::string last_msg;
  ebus::LogLevel last_level = ebus::LogLevel::none;

  logger.setSink([&](ebus::LogLevel level, std::string_view msg) {
    last_level = level;
    last_msg = msg;
  });

  SECTION("Level filtering") {
    logger.setLevel(ebus::LogLevel::error);

    logger.log(ebus::LogLevel::debug, "invisible");
    REQUIRE(last_msg.empty());

    logger.log(ebus::LogLevel::error, "visible");
    REQUIRE(last_msg == "visible");
  }

  SECTION("Disabling logging") {
    logger.setLevel(ebus::LogLevel::none);
    logger.log(ebus::LogLevel::error, "should not see this");
    REQUIRE(last_level != ebus::LogLevel::error);
  }

  logger.setSink(nullptr);  // Cleanup
}
