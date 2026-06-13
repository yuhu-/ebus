/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/detail/config_validator.hpp>
#include <ebus/detail/json_reader.hpp>
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

  SECTION("Deep recursion and stack safety test") {
    result.clear();
    {
      ebus::detail::JsonWriter writer(visitor);
      // Manually trigger deep nesting
      writer.startObject();
      for (int i = 0; i < 10; ++i) {
        writer.appendKey("nest");
        writer.startObject();
      }
      writer.writeField("leaf", true);
      for (int i = 0; i < 10; ++i) {
        writer.endObject();
      }
      writer.endObject();
    }
    // Check that we didn't crash and the leaf is reachable
    REQUIRE(result.find("\"leaf\":true") != std::string::npos);
    REQUIRE(std::count(result.begin(), result.end(), '{') == 11);
    REQUIRE(std::count(result.begin(), result.end(), '}') == 11);
  }
}

TEST_CASE("JSON Utils: Pull-Parser Extraction", "[utils][json]") {
  std::string_view json = R"({
    "bus": { "window_us": 4500, "active": true },
    "meta": "none",
    "list": [10, 20]
  })";

  SECTION("Path-based extraction with get()") {
    ebus::detail::JsonReader reader(json);
    REQUIRE(reader.get("bus.window_us") ==
            ebus::detail::JsonReader::Token::number);
    REQUIRE(reader.asNum<int>() == 4500);

    // Reset reader and search another path
    ebus::detail::JsonReader reader2(json);
    REQUIRE(reader2.get("bus.active") ==
            ebus::detail::JsonReader::Token::boolean);
    REQUIRE(reader2.asBool() == true);

    ebus::detail::JsonReader reader3(json);
    REQUIRE(reader3.get("meta") == ebus::detail::JsonReader::Token::string);
    REQUIRE(reader3.value() == "none");
  }

  SECTION("Path-based extraction with array indexing") {
    std::string_view array_json = R"({
      "data_points": [
        {"id": 1, "value": 100},
        {"id": 2, "value": 200}
      ],
      "names": ["alpha", "beta"]
    })";

    ebus::detail::JsonReader reader(array_json);
    REQUIRE(reader.get("data_points.0.value") ==
            ebus::detail::JsonReader::Token::number);
    REQUIRE(reader.asNum<int>() == 100);
  }

  SECTION("mergeFromJson ignores unknown keys and updates specific fields") {
    ebus::RuntimeConfig cfg;
    cfg.address = 0x10;
    cfg.bus.window_us = 4000;

    // JSON containing a mix of valid updates, unknown keys, and nested noise
    std::string update_json = R"({
      "address": 31,
      "legacy_field_to_ignore": 999,
      "bus": {
        "window_us": 4500,
        "noise": { "deep": true }
      },
      "new_feature_flag": "future_proof"
    })";

    bool success = cfg.mergeFromJson(update_json);
    REQUIRE(success);
    REQUIRE(cfg.address == 31);
    REQUIRE(cfg.bus.window_us == 4500);
    // Verify defaults weren't wiped for missing keys (default lock_counter is
    // 3)
    REQUIRE(cfg.lock_counter == 3);
  }
}

TEST_CASE("JSON Reader: Find and Reset", "[utils][json]") {
  std::string_view json = "[10, 20, 30, 40]";
  JsonReader reader(json);

  SECTION("reset() returns to the beginning") {
    REQUIRE(reader.next() == JsonReader::Token::array_start);
    REQUIRE(reader.next() == JsonReader::Token::number);
    REQUIRE(reader.asNum<int>() == 10);

    reader.reset();
    REQUIRE(reader.next() == JsonReader::Token::array_start);
    REQUIRE(reader.next() == JsonReader::Token::number);
    REQUIRE(reader.asNum<int>() == 10);
  }

  SECTION("find() searches elements in an array") {
    REQUIRE(reader.next() == JsonReader::Token::array_start);

    // Search for 30
    bool found = reader.find([](JsonReader& r) {
      // Each iteration of find rewinds to the start of the element.
      // So next() here gets the current element's token type.
      return r.next() == JsonReader::Token::number && r.asNum<int>() == 30;
    });
    REQUIRE(found);
    REQUIRE(reader.asNum<int>() == 30);

    // Search for next element (40)
    bool found_next = reader.find([](JsonReader& r) {
      return r.next() == JsonReader::Token::number && r.asNum<int>() == 40;
    });
    REQUIRE(found_next);
    REQUIRE(reader.asNum<int>() == 40);

    // Reach end
    REQUIRE(!reader.find([](JsonReader&) { return true; }));
  }

  SECTION("find() with nested objects in array") {
    std::string_view obj_array = R"([{"id":1}, {"id":2}, {"id":3}])";
    JsonReader r(obj_array);
    REQUIRE(r.next() == JsonReader::Token::array_start);

    bool found = r.find([](JsonReader& inner) {
      if (inner.next() != JsonReader::Token::object_start) return false;
      if (inner.findKey("id")) {
        inner.next();
        return inner.asNum<int>() == 2;
      }
      return false;
    });

    REQUIRE(found);
    // Reader should be positioned where the predicate left it (at the number 2)
    REQUIRE(r.asNum<int>() == 2);
    REQUIRE(r.next() == JsonReader::Token::object_end);
  }
}

TEST_CASE("JSON Reader: forEachField", "[utils][json]") {
  std::string_view json =
      R"({"a": 1, "b": [1, 2], "c": {"inner": true}, "d": 4})";
  JsonReader reader(json);
  REQUIRE(reader.next() == JsonReader::Token::object_start);

  int count = 0;
  reader.forEachField([&](std::string_view key, JsonReader& r) {
    count++;
    if (key == "a") {
      REQUIRE(r.next() == JsonReader::Token::number);
      REQUIRE(r.asNum<int>() == 1);
      return true;
    }
    if (key == "d") {
      REQUIRE(r.next() == JsonReader::Token::number);
      REQUIRE(r.asNum<int>() == 4);
      return true;
    }
    return false;  // auto-skip b and c
  });
  REQUIRE(count == 4);
  REQUIRE(reader.next() == JsonReader::Token::end);
}

TEST_CASE("JSON Utils: Pretty Printing", "[utils][json]") {
  std::string result;
  auto visitor = [&](std::string_view chunk) { result.append(chunk); };

  SECTION("Basic pretty object") {
    ebus::detail::JsonWriter writer(visitor, true);
    writer.startObject();
    writer.writeField("a", 1);
    writer.writeField("b", true);
    writer.endObject();
    REQUIRE(result == "{\n  \"a\": 1,\n  \"b\": true\n}");
  }

  SECTION("Nested pretty structures") {
    ebus::detail::JsonWriter writer(visitor, true);
    writer.startObject();
    writer.appendKey("list");
    writer.startArray();
    writer.writeValue(10);
    writer.endArray();
    writer.endObject();
    REQUIRE(result == "{\n  \"list\": [\n    10\n  ]\n}");
  }
}

TEST_CASE("JSON Reader: Strict Validation", "[utils][json]") {
  SECTION("Valid JSON") {
    REQUIRE(JsonReader::validate("{}"));
    REQUIRE(JsonReader::validate("[]"));
    REQUIRE(JsonReader::validate(
        R"({"key": "val", "num": 123, "arr": [1, 2, 3]})"));
    REQUIRE(JsonReader::validate(R"({"obj": {"inner": true}, "empty": []})"));
  }

  SECTION("Invalid JSON - Structural") {
    REQUIRE_FALSE(JsonReader::validate("{"));
    REQUIRE_FALSE(JsonReader::validate("}"));
    REQUIRE_FALSE(JsonReader::validate(R"({"missing": "colon" "value"})"));
    REQUIRE_FALSE(
        JsonReader::validate(R"({"extra": "comma",})"));  // Trailing comma
    REQUIRE_FALSE(JsonReader::validate(
        R"({"key": "val" "key2": "val2"})"));  // Missing comma
    REQUIRE_FALSE(
        JsonReader::validate(R"({"bad": [1, 2,})"));  // Trailing comma in array
    REQUIRE_FALSE(JsonReader::validate("[1, 2 3]"));  // Missing comma in array
  }

  SECTION("Invalid JSON - Types") {
    REQUIRE_FALSE(JsonReader::validate(R"({123: "number key"})"));
    REQUIRE_FALSE(JsonReader::validate(R"({"bad": tru})"));
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

TEST_CASE("ConfigValidator: validateJson hex strings", "[app][config][json]") {
  SECTION("Valid decimal address") {
    std::string_view json = R"({"address": 49})";  // 0x31
    REQUIRE(ConfigValidator::validateJson(json));
  }
  SECTION("Valid hex address string") {
    std::string_view json = R"({"address": "0x31"})";
    REQUIRE(ConfigValidator::validateJson(json));
  }
  SECTION("Invalid hex address string (slave)") {
    std::string_view json = R"({"address": "0x15"})";
    REQUIRE_FALSE(ConfigValidator::validateJson(json));
  }
  SECTION("Hex string for nested field") {
    // window_us: 4500 = 0x1194
    std::string_view json = R"({"bus": {"window_us": "0x1194"}})";
    REQUIRE(ConfigValidator::validateJson(json));
  }
  SECTION("Illegal characters in hex string") {
    // Trailing 'G' is not a hex digit
    std::string_view json = R"({"address": "0x31G"})";
    REQUIRE_FALSE(ConfigValidator::validateJson(json));
  }
  SECTION("Illegal characters in decimal string") {
    // Trailing 'Z' is not a digit
    std::string_view json = R"({"address": "49Z"})";
    REQUIRE_FALSE(ConfigValidator::validateJson(json));
  }
  SECTION("Malformed hex prefix") {
    // Just "0x" with no digits is invalid
    std::string_view json = R"({"address": "0x"})";
    REQUIRE_FALSE(ConfigValidator::validateJson(json));
  }
}
