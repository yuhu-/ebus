/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <ebus/callbacks.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/device.hpp>
#include <ebus/utils.hpp>
#include <string>
#include <vector>

using namespace ebus::detail;

TEST_CASE("JSON Utils: Serialization and Escaping", "[utils][json]") {
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

TEST_CASE("JSON Utils: Streaming Writer", "[utils][json]") {
  std::string result;
  auto visitor = [&](std::string_view chunk) { result.append(chunk); };

  SECTION("Object with mixed types") {
    result.clear();
    {
      ebus::detail::JsonWriter writer(visitor);
      writer.startObject();
      writer.writeField("string", "hello \"world\"");
      writer.writeField("bool", true);
      writer.writeField("int", static_cast<int64_t>(-123));
      writer.writeField("uint", static_cast<uint64_t>(456));
      writer.writeFieldFloat("float", 12.345f, 1);
      writer.writeHexField("hex", ebus::ByteView({0xDE, 0xAD}));
      writer.endObject();
    }
    // Note: JsonWriter handles escaping internally via writeEscaped
    CAPTURE(result);
    REQUIRE(result.find("\"string\":\"hello \\\"world\\\"\"") !=
            std::string::npos);
    REQUIRE(result.find("\"bool\":true") != std::string::npos);
    REQUIRE(result.find("\"int\":-123") != std::string::npos);
    REQUIRE(result.find("\"uint\":456") != std::string::npos);
    REQUIRE(result.find("\"float\":12.3") != std::string::npos);
    REQUIRE(result.find("\"hex\":\"dead\"") != std::string::npos);
  }

  SECTION("Nested arrays and objects") {
    result.clear();
    {
      ebus::detail::JsonWriter writer(visitor);
      writer.startObject();
      writer.appendKey("list");
      writer.startArray();
      writer.write("1");
      writer.write(",");
      writer.write("2");
      writer.endArray();
      writer.endObject();
    }
    REQUIRE(result == "{\"list\":[1,2]}");
  }

  SECTION("Deeply nested commas and delimiters") {
    result.clear();
    {
      ebus::detail::JsonWriter writer(visitor);
      writer.startObject();
      writer.writeField("outer_val", 1);

      writer.appendKey("nested_obj");
      writer.startObject();
      writer.writeField("inner_val", 2);
      writer.endObject();

      writer.appendKey("nested_arr");
      writer.startArray();
      // Manual calls to startObject in array (simulating user loops)
      writer.startObject();
      writer.writeField("id", 1);
      writer.endObject();

      writer.startObject();
      writer.writeField("id", 2);
      writer.endObject();
      writer.endArray();

      writer.writeField("tail", 3);
      writer.endObject();
    }
    // Verify exact sequence of commas and braces
    REQUIRE(result ==
            "{\"outer_val\":1,\"nested_obj\":{\"inner_val\":2},\"nested_arr\":["
            "{\"id\":1},{\"id\":2}],\"tail\":3}");
  }
}

TEST_CASE("JSON Utils: Parsing Helpers", "[utils][json]") {
  std::string_view json =
      R"({"key":"val","num":123,"nested":{"sub":true},"arr":[1,2,3]})";

  SECTION("Extract simple value") {
    REQUIRE(ebus::extract(json, "key") == "val");
    REQUIRE(ebus::extract(json, "num") == "123");
  }

  SECTION("Extract sub-object") {
    REQUIRE(ebus::extractSub(json, "nested") == "{\"sub\":true}");
  }

  SECTION("toNum conversions") {
    REQUIRE(ebus::toNum<int>("123") == 123);
    REQUIRE(ebus::toNum<uint32_t>("456") == 456);
    REQUIRE(ebus::toNum<int>("") == 0);
    REQUIRE(ebus::toNum<int>("null") == 0);
  }
}

TEST_CASE("JSON Utils: Escaping", "[utils][json]") {
  REQUIRE(ebus::escapeJson("hello") == "hello");
  REQUIRE(ebus::escapeJson("\"quotes\"") == "\\\"quotes\\\"");
  REQUIRE(ebus::escapeJson("new\nline") == "new\\nline");
  // Control character (0x01)
  REQUIRE(ebus::escapeJson(std::string(1, 0x01)) == "\\u0001");
}
