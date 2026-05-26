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
  SECTION("Device info to JSON") {
    ebus::DeviceInfo info;
    info.slave_address = 0x15;
    info.manufacturer_name = "Vaillant";
    info.vaillant.serial_number = "2112345678901234567890123456";

    std::string json = ebus::toJson(info, 512);
    REQUIRE(json.find("\"slave_address\":\"15\"") != std::string::npos);
    REQUIRE(json.find("\"vaillant\":{") != std::string::npos);
  }
}
