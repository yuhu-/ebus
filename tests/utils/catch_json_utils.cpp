/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/callbacks.hpp>
#include <ebus/device.hpp>
#include <ebus/utils.hpp>

using namespace ebus::detail;

TEST_CASE("JsonUtils: Serialization and Escaping", "[utils][json]") {
  SECTION("Escaping special characters") {
    ebus::ErrorInfo info;
    info.message = "Error \"quoted\" \n newline";
    info.utilization = 12.345;

    std::string json = ebus::toJson(info);
    REQUIRE(json.find("Error \\\"quoted\\\" \\n newline") != std::string::npos);
    REQUIRE(json.find("12.35") != std::string::npos);  // Rounded
  }

  SECTION("Device info to JSON") {
    ebus::DeviceInfo info;
    info.slave_address = 0x15;
    info.manufacturer_name = "Vaillant";
    info.vaillant.serial_number = "2112345678901234567890123456";

    std::string json = ebus::toJson(info);
    REQUIRE(json.find("\"slave_address\":21") != std::string::npos);
    REQUIRE(json.find("\"vaillant\":{") != std::string::npos);
  }
}
