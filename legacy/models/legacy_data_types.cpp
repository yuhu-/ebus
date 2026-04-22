/*
 * Copyright (C) 2017-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <ebus/data_types.hpp>
#include <ebus/definitions.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <iostream>
#include <limits>
#include <vector>

using namespace ebus;

void test_bcd() {
  for (uint8_t v = 0; v <= 99; ++v) {
    Sequence bytes = ebus::encode(ebus::DataType::bcd, v);
    auto decoded = ebus::decode(ebus::DataType::bcd, bytes);
    assert(decoded && ebus::asInt64(*decoded) == v);
  }
  // Invalid BCD
  std::vector<uint8_t> invalid = {0xaa};
  assert(!ebus::decode(ebus::DataType::bcd, invalid).has_value());
}

void test_uint8() {
  for (uint16_t v = 0; v <= 0xff; ++v) {
    uint8_t value = static_cast<uint8_t>(v);
    Sequence bytes = ebus::encode(DataType::uint8, value);
    auto decoded = ebus::decode(DataType::uint8, bytes);
    if (v == 0xff) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
    assert(bytes.size() == 1 && bytes[0] == value);
  }
}

void test_int8() {
  for (int16_t v = -128; v <= 127; ++v) {
    int8_t value = static_cast<int8_t>(v);
    Sequence bytes = ebus::encode(DataType::int8, value);
    auto decoded = ebus::decode(DataType::int8, bytes);
    if (v == -128) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_data1b() {
  for (int16_t v = -128; v <= 127; v += 17) {
    double_t value = static_cast<double_t>(v);
    Sequence bytes = ebus::encode(DataType::data1b, value);
    auto decoded = ebus::decode(DataType::data1b, bytes);
    if (v == -128) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }
}

void test_data1c() {
  for (uint16_t v = 0; v <= 255; v += 17) {
    double_t value = static_cast<double_t>(v) / 2.0;
    Sequence bytes = ebus::encode(DataType::data1c, value);
    auto decoded = ebus::decode(DataType::data1c, bytes);
    if (v == 255) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }
}

void test_data1c_resolution() {
  // DATA1C: 0.5 resolution
  // 0x01 -> 0.5
  auto d1 = ebus::decode(DataType::data1c, ebus::ByteView({0x01}));
  assert(d1 && std::fabs(ebus::asDouble(*d1) - 0.5) < 1e-5);

  // 0x02 -> 1.0
  auto d2 = ebus::decode(DataType::data1c, ebus::ByteView({0x02}));
  assert(d2 && std::fabs(ebus::asDouble(*d2) - 1.0) < 1e-5);
}

void test_uint16() {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {  // step to keep test fast
    uint16_t value = static_cast<uint16_t>(v);
    Sequence bytes = ebus::encode(DataType::uint16, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint16, bytes, Endian::little);
    if (v == 0xffff) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_uint16r() {
  for (uint32_t v = 0; v <= 0xffff; v += 257) {  // step to keep test fast
    uint16_t value = static_cast<uint16_t>(v);
    Sequence bytes = ebus::encode(DataType::uint16r, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint16r, bytes, Endian::little);
    if (v == 0xffff) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_int16() {
  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    Sequence bytes = ebus::encode(DataType::int16, value, Endian::little);
    auto decoded = ebus::decode(DataType::int16, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_int16r() {
  for (int32_t v = -32768; v <= 32767; v += 513) {
    int16_t value = static_cast<int16_t>(v);
    Sequence bytes = ebus::encode(DataType::int16r, value, Endian::little);
    auto decoded = ebus::decode(DataType::int16r, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_data2b() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2b, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2b, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }
}

void test_data2br() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 256.0;
    Sequence bytes = ebus::encode(DataType::data2br, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2br, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }
}

void test_data2c() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2c, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2c, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }
}

void test_data2cr() {
  for (int32_t v = -32768; v <= 32767; v += 4096) {
    double_t value = static_cast<double_t>(v) / 16.0;
    Sequence bytes = ebus::encode(DataType::data2cr, value, Endian::little);
    auto decoded = ebus::decode(DataType::data2cr, bytes, Endian::little);
    if (v == -32768) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-3);
    }
  }
}

void test_uint32() {
  std::vector<uint32_t> test_values = {0, 1, 0xffffffff, 0x12345678,
                                       0x80000000};
  for (uint32_t value : test_values) {
    Sequence bytes = ebus::encode(DataType::uint32, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint32, bytes, Endian::little);
    if (value == 0xffffffff) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == static_cast<int64_t>(value));
    }
  }
}

void test_uint32r() {
  std::vector<uint32_t> test_values = {0, 1, 0xffffffff, 0x12345678,
                                       0x80000000};
  for (uint32_t value : test_values) {
    Sequence bytes = ebus::encode(DataType::uint32r, value, Endian::little);
    auto decoded = ebus::decode(DataType::uint32r, bytes, Endian::little);
    if (value == 0xffffffff) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == static_cast<int64_t>(value));
    }
  }
}

void test_int32() {
  std::vector<int32_t> test_values = {0,
                                      1,
                                      -1,
                                      std::numeric_limits<int32_t>::max(),
                                      std::numeric_limits<int32_t>::min(),
                                      0x12345678};
  for (int32_t value : test_values) {
    Sequence bytes = ebus::encode(DataType::int32, value, Endian::little);
    auto decoded = ebus::decode(DataType::int32, bytes, Endian::little);
    if (value == std::numeric_limits<int32_t>::min()) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_int32r() {
  std::vector<int32_t> test_values = {0,
                                      1,
                                      -1,
                                      std::numeric_limits<int32_t>::max(),
                                      std::numeric_limits<int32_t>::min(),
                                      0x12345678};
  for (int32_t value : test_values) {
    Sequence bytes = ebus::encode(DataType::int32r, value, Endian::little);
    auto decoded = ebus::decode(DataType::int32r, bytes, Endian::little);
    if (value == std::numeric_limits<int32_t>::min()) {
      assert(decoded && ebus::isNull(*decoded));
    } else {
      assert(decoded && ebus::asInt64(*decoded) == value);
    }
  }
}

void test_float() {
  std::vector<float> test_values = {0.0f,
                                    1.0f,
                                    -1.0f,
                                    123.456f,
                                    -789.123f,
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity(),
                                    std::nanf("")};
  for (float value : test_values) {
    Sequence bytes = ebus::encode(DataType::float4,
                                  static_cast<double_t>(value), Endian::little);
    auto decoded = ebus::decode(DataType::float4, bytes, Endian::little);
    if (std::isnan(value)) {
      assert(decoded && std::isnan(ebus::asDouble(*decoded)));
    } else if (std::isinf(value)) {
      assert(decoded && std::isinf(ebus::asDouble(*decoded)) &&
             (std::signbit(value) == std::signbit(ebus::asDouble(*decoded))));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }
}

void test_floatr() {
  std::vector<float> test_values = {0.0f,
                                    1.0f,
                                    -1.0f,
                                    123.456f,
                                    -789.123f,
                                    std::numeric_limits<float>::infinity(),
                                    -std::numeric_limits<float>::infinity(),
                                    std::nanf("")};
  for (float value : test_values) {
    Sequence bytes = ebus::encode(DataType::float4r,
                                  static_cast<double_t>(value), Endian::little);
    auto decoded = ebus::decode(DataType::float4r, bytes, Endian::little);
    if (std::isnan(value)) {
      assert(decoded && std::isnan(ebus::asDouble(*decoded)));
    } else if (std::isinf(value)) {
      assert(decoded && std::isinf(ebus::asDouble(*decoded)) &&
             (std::signbit(value) == std::signbit(ebus::asDouble(*decoded))));
    } else {
      assert(decoded && std::fabs(ebus::asDouble(*decoded) - value) < 1e-5);
    }
  }
}

void test_char() {
  std::vector<std::string> test_strings = {"", "A", "Hello", "ebus123",
                                           std::string(8, 'Z')};
  for (const std::string& str : test_strings) {
    Sequence bytes = ebus::encode(DataType::char8, str);
    auto decoded = ebus::decode(DataType::char8, bytes);
    assert(decoded && std::get<std::string>(*decoded).find(str) == 0);
  }
}

void test_hex() {
  std::vector<std::string> test_strings = {
      "", "00", "FF", "1234", "abcdef", "ABCDEF", "deadbeef"};
  for (const std::string& str : test_strings) {
    std::string str_lower = str;
    std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(),
                   ::tolower);

    Sequence bytes = ebus::encode(DataType::hex8, str_lower);
    auto decoded = ebus::decode(DataType::hex8, bytes);
    assert(decoded);
    assert(std::get<std::string>(*decoded).find(str_lower) == 0);
  }
}

void test_supported_types_metadata() {
  auto types = ebus::getSupportedDataTypes();
  assert(!types.empty());
  for (auto dt : types) {
    auto meta = ebus::getMeta(dt);
    assert(meta.has_value());
    assert(meta->dt == dt);
    assert(std::string(meta->name) != "UNKNOWN");
    assert(meta->size == ebus::sizeOfDataType(dt));
  }
}

void test_auto_detect() {
  // uint8
  DataValue v1 = static_cast<uint8_t>(42);
  Sequence s1 = ebus::encode(DataType::auto_detect, v1);
  assert(s1.size() == 1 && s1[0] == 42);

  // float4 (default for double)
  DataValue v2 = 21.5;
  Sequence s2 = ebus::encode(DataType::auto_detect, v2);
  assert(s2.size() == 4);
  auto d2 = ebus::decode(DataType::float4, s2);
  assert(d2 && std::fabs(ebus::asDouble(*d2) - 21.5) < 1e-5);

  // char8 (default for string)
  DataValue v3 = std::string("EBUS");
  Sequence s3 = ebus::encode(DataType::auto_detect, v3);
  assert(s3.size() == 8 &&
         std::string(reinterpret_cast<const char*>(s3.data()), 4) == "EBUS");
}

void test_get_as() {
  // Valid conversion
  DataValue v1 = static_cast<int32_t>(200);
  auto r1 = ebus::getAs<uint8_t>(v1);
  assert(r1.has_value() && *r1 == 200);

  // Overflow check
  DataValue v2 = static_cast<int32_t>(300);
  auto r2 = ebus::getAs<uint8_t>(v2);
  assert(!r2.has_value());

  // Underflow check
  DataValue v3 = static_cast<int32_t>(-1);
  auto r3 = ebus::getAs<uint8_t>(v3);
  assert(!r3.has_value());
}

void test_encode_range_checking() {
  // uint8 range is 0-255. 256 is out of range.
  assert(ebus::encode(DataType::uint8, 255).size() == 1);
  assert(ebus::encode(DataType::uint8, 256).empty());
  assert(ebus::encode(DataType::uint8, -1).empty());

  // int8 range is -128 to 127.
  assert(ebus::encode(DataType::int8, -128).size() == 1);
  assert(ebus::encode(DataType::int8, 127).size() == 1);
  assert(ebus::encode(DataType::int8, 128).empty());

  // uint16 range is 0-65535.
  assert(ebus::encode(DataType::uint16, 65536).empty());
}

void test_isValid() {
  // BCD validation
  assert(ebus::isValid(DataType::bcd, ebus::ByteView({0x12})));  // "12"
  assert(
      !ebus::isValid(DataType::bcd, ebus::ByteView({0x1A})));  // Invalid nibble
  assert(ebus::isValid(
      DataType::bcd,
      ebus::ByteView(
          {0xFF})));  // Spec Replacement Value is valid protocol data

  // Size validation
  assert(
      !ebus::isValid(DataType::uint16, ebus::ByteView({0x01})));  // Too short
}

void test_sentinels() {
  // BCD 0xFF -> null
  auto v1 = ebus::decode(DataType::bcd, ebus::ByteView({0xff}));
  assert(v1 && ebus::isNull(*v1));

  // DATA1B 0x80 -> null
  auto v2 = ebus::decode(DataType::data1b, ebus::ByteView({0x80}));
  assert(v2 && ebus::isNull(*v2));

  // DATA1C 0xFF -> null
  auto v3 = ebus::decode(DataType::data1c, ebus::ByteView({0xff}));
  assert(v3 && ebus::isNull(*v3));

  // UINT16 0xFFFF -> null
  auto v4 = ebus::decode(DataType::uint16, ebus::ByteView({0xff, 0xff}));
  assert(v4 && ebus::isNull(*v4));
}

void test_isNumeric() {
  assert(ebus::isNumeric(DataType::uint8) == true);
  assert(ebus::isNumeric(DataType::int8) == true);
  assert(ebus::isNumeric(DataType::float4) == true);
  assert(ebus::isNumeric(DataType::data1b) == true);
  assert(ebus::isNumeric(DataType::bcd) == true);
  assert(ebus::isNumeric(DataType::uint16) == true);
  assert(ebus::isNumeric(DataType::int16) == true);
  assert(ebus::isNumeric(DataType::uint32) == true);
  assert(ebus::isNumeric(DataType::int32) == true);
  assert(ebus::isNumeric(DataType::data1c) == true);
  assert(ebus::isNumeric(DataType::data2b) == true);

  assert(ebus::isNumeric(DataType::char1) == false);
  assert(ebus::isNumeric(DataType::char8) == false);
  assert(ebus::isNumeric(DataType::hex1) == false);
  assert(ebus::isNumeric(DataType::hex8) == false);
  assert(ebus::isNumeric(DataType::error) == false);
  assert(ebus::isNumeric(DataType::auto_detect) == false);
}

void test_null_sentinel_encoding() {
  // BCD has replacement_value 0xff
  Sequence bcd_null = ebus::encode(DataType::bcd, ebus::nullValue());
  assert(bcd_null.size() == 1);
  assert(bcd_null[0] == 0xff);

  // uint8 has replacement_value 0xff
  Sequence uint8_null = ebus::encode(DataType::uint8, ebus::nullValue());
  assert(uint8_null.size() == 1);
  assert(uint8_null[0] == 0xff);

  // int8 has replacement_value 0x80
  Sequence int8_null = ebus::encode(DataType::int8, ebus::nullValue());
  assert(int8_null.size() == 1);
  assert(int8_null[0] == 0x80);

  // data1b has replacement_value 0x80
  Sequence data1b_null = ebus::encode(DataType::data1b, ebus::nullValue());
  assert(data1b_null.size() == 1);
  assert(data1b_null[0] == 0x80);

  // data1c has replacement_value 0xff
  Sequence data1c_null = ebus::encode(DataType::data1c, ebus::nullValue());
  assert(data1c_null.size() == 1);
  assert(data1c_null[0] == 0xff);

  // uint16 has replacement_value 0xffff
  Sequence uint16_null = ebus::encode(DataType::uint16, ebus::nullValue());
  assert(uint16_null.size() == 2);
  assert(uint16_null[0] == 0xff);
  assert(uint16_null[1] == 0xff);

  // int16 has replacement_value 0x8000 (little-endian: 0x00 0x80)
  Sequence int16_null = ebus::encode(DataType::int16, ebus::nullValue());
  assert(int16_null.size() == 2);
  assert(int16_null[0] == 0x00);
  assert(int16_null[1] == 0x80);

  // uint32 has replacement_value 0xffffffff
  Sequence uint32_null = ebus::encode(DataType::uint32, ebus::nullValue());
  assert(uint32_null.size() == 4);
  assert(uint32_null[0] == 0xff);
  assert(uint32_null[1] == 0xff);
  assert(uint32_null[2] == 0xff);
  assert(uint32_null[3] == 0xff);

  // int32 has replacement_value 0x80000000 (little-endian: 0x00 0x00 0x00 0x80)
  Sequence int32_null = ebus::encode(DataType::int32, ebus::nullValue());
  assert(int32_null.size() == 4);
  assert(int32_null[0] == 0x00);
  assert(int32_null[1] == 0x00);
  assert(int32_null[2] == 0x00);
  assert(int32_null[3] == 0x80);

  // float4 does not have replacement_value, so encoding null should return
  // empty sequence
  Sequence float4_null = ebus::encode(DataType::float4, ebus::nullValue());
  assert(float4_null.empty());

  // char1 does not have replacement_value, so encoding null should return empty
  // sequence
  Sequence char1_null = ebus::encode(DataType::char1, ebus::nullValue());
  assert(char1_null.empty());
}

void test_all_types_roundtrip() {
  auto types = ebus::getSupportedDataTypes();
  for (auto dt : types) {
    auto meta = ebus::getMeta(dt);
    assert(meta.has_value());

    // 1. Metadata check
    assert(meta->size == ebus::sizeOfDataType(dt));

    // 2. Validity check with zero-buffer
    std::vector<uint8_t> buffer(meta->size, 0);
    assert(ebus::isValid(dt, buffer));

    // 3. Decode -> Encode roundtrip
    auto decoded = ebus::decode(dt, buffer);
    assert(decoded.has_value());

    Sequence encoded = ebus::encode(dt, *decoded);
    assert(encoded.size() == meta->size);

    // 4. Data integrity check
    // For standard types with 1.0 factor and no float precision loss,
    // the value must be identical.
    if (!meta->is_float && std::abs(meta->factor - 1.0) < 1e-9) {
      auto re_decoded = ebus::decode(dt, encoded);
      assert(re_decoded.has_value());
      // variant operator== checks type and value
      assert(*re_decoded == *decoded);
    }
  }
}

int main() {
  test_bcd();
  test_uint8();
  test_int8();
  test_data1b();
  test_data1c();
  test_data1c_resolution();
  test_uint16();
  test_uint16r();
  test_int16();
  test_int16r();
  test_data2b();
  test_data2br();
  test_data2c();
  test_data2cr();
  test_uint32();
  test_uint32r();
  test_int32();
  test_int32r();
  test_float();
  test_floatr();
  test_char();
  test_hex();
  test_supported_types_metadata();
  test_auto_detect();
  test_get_as();
  test_encode_range_checking();
  test_isValid();
  test_sentinels();
  test_isNumeric();
  test_null_sentinel_encoding();
  test_all_types_roundtrip();

  std::cout << "All datatype conversion tests passed!" << std::endl;
  return 0;
}