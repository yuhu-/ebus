/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstddef>
#include <ebus/data_types.hpp>
#include <ebus/sequence.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "core/telegram.hpp"

void printSequence(const std::string& strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::toVector(strSequence));

  std::cout << "sequence: " << sequence.toString() << std::endl;
  std::cout << "     crc: " << ebus::toString(sequence.crc()) << std::endl
            << std::endl;
}

void printMasterSlave(const std::string& strMaster,
                      const std::string& strSlave) {
  ebus::Sequence master;
  master.assign(ebus::toVector(strMaster));

  ebus::Sequence slave;
  slave.assign(ebus::toVector(strSlave));

  ebus::detail::Telegram tel;
  tel.createMaster(master);
  tel.createSlave(slave);

  std::cout << "  master: " << tel.toStringMaster() << std::endl;
  std::cout << "     crc: " << ebus::toString(tel.getMasterCRC()) << std::endl;
  std::cout << "   slave: " << tel.toStringSlave() << std::endl;
  std::cout << "     crc: " << ebus::toString(tel.getSlaveCRC()) << std::endl
            << std::endl;
}

void createTelegram(const std::string& strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::toVector(strSequence));

  ebus::detail::Telegram tel(sequence);

  std::ostringstream ostr;
  ostr << "telegram: " << sequence.toString() << std::endl;

  ostr << "telegram: " << tel.getMaster().toString() << "    "
       << tel.getSlave().toString() << std::endl;

  std::cout << ostr.str() << std::endl;
}

void checkTargetMasterSlave() {
  for (int b0 = 0x00; b0 <= 0xff; b0++) {
    std::cout << "i: 0x" << ebus::toString(b0);

    if (ebus::isTarget(b0)) std::cout << " isTarget";

    if (ebus::isMaster(b0)) std::cout << " isMaster";

    if (ebus::isSlave(b0)) std::cout << " isSlave";

    if (b0 != ebus::slaveOf(b0))
      std::cout << " ==> slaveOf  = 0x" << ebus::toString(ebus::slaveOf(b0));

    if (b0 != ebus::masterOf(b0))
      std::cout << "  ==> masterOf = 0x" << ebus::toString(ebus::masterOf(b0));

    std::cout << std::endl;
  }
}

void encode(uint8_t c, uint8_t d, uint8_t (&data)[2]) {
  data[0] = 0xC0 | (c << 2) | ((d & 0xC0) >> 6);
  data[1] = 0x80 | (d & 0x3F);
}

void decode(uint8_t c, uint8_t d, uint8_t (&data)[2]) {
  data[0] = (c >> 2) & 0x0F;
  data[1] = ((c & 0x03) << 6) | (d & 0x3F);
}

void printDecodeEncode(uint8_t c, uint8_t d) {
  std::cout << "Original: 0x" << ebus::toString(c) << " 0x" << ebus::toString(d)
            << std::endl;

  uint8_t decoded[2];
  decode(c, d, decoded);
  std::cout << "Decoded:  0x" << ebus::toString(decoded[0]) << " 0x"
            << ebus::toString(decoded[1]) << std::endl;

  uint8_t encoded[2];
  encode(decoded[0], decoded[1], encoded);
  std::cout << "Encoded:  0x" << ebus::toString(encoded[0]) << " 0x"
            << ebus::toString(encoded[1]) << std::endl
            << std::endl;
}

void printEncodeDecode(uint8_t c, uint8_t d) {
  std::cout << "Original: 0x" << ebus::toString(c) << " 0x" << ebus::toString(d)
            << std::endl;

  uint8_t encoded[2];
  encode(c, d, encoded);
  std::cout << "Encoded:  0x" << ebus::toString(encoded[0]) << " 0x"
            << ebus::toString(encoded[1]) << std::endl;

  uint8_t decoded[2];
  decode(encoded[0], encoded[1], decoded);
  std::cout << "Decoded:  0x" << ebus::toString(decoded[0]) << " 0x"
            << ebus::toString(decoded[1]) << std::endl
            << std::endl;
}

void printFloatTestLittleEndian() {
  float f = 21.0375f;
  ebus::Sequence ef =
      ebus::encode(ebus::DataType::float4, f, ebus::Endian::little);
  std::cout << "float: " << f << " to bytes: " << ebus::toString(ef)
            << std::endl;
  auto df = ebus::decode(ebus::DataType::float4, ef, ebus::Endian::little);
  std::cout << "bytes: " << ebus::toString(ef)
            << " to float: " << ebus::asDouble(*df) << std::endl
            << std::endl;

  std::vector<uint8_t> h = {0xcd, 0x4c, 0xb2, 0x41};
  auto dh = ebus::decode(ebus::DataType::float4, h, ebus::Endian::little);
  std::cout << "bytes: " << ebus::toString(h)
            << " to float: " << ebus::asDouble(*dh) << std::endl;
  ebus::Sequence bh =
      ebus::encode(ebus::DataType::float4, *dh, ebus::Endian::little);
  std::cout << "float: " << ebus::asDouble(*dh)
            << " to bytes: " << ebus::toString(bh) << std::endl
            << std::endl;
}

void printFloatTestBigEndian() {
  float f = 21.0375f;
  ebus::Sequence ef =
      ebus::encode(ebus::DataType::float4, f, ebus::Endian::big);
  std::cout << "float: " << f << " to bytes: " << ebus::toString(ef)
            << std::endl;
  auto df = ebus::decode(ebus::DataType::float4, ef, ebus::Endian::big);
  std::cout << "bytes: " << ebus::toString(ef)
            << " to float: " << ebus::asDouble(*df) << std::endl
            << std::endl;

  std::vector<uint8_t> h = {0x41, 0xa8, 0x4c, 0x7f};
  auto dh = ebus::decode(ebus::DataType::float4, h, ebus::Endian::big);
  std::cout << "bytes: " << ebus::toString(h)
            << " to float: " << ebus::asDouble(*dh) << std::endl;
  ebus::Sequence bh =
      ebus::encode(ebus::DataType::float4, *dh, ebus::Endian::big);
  std::cout << "float: " << ebus::asDouble(*dh)
            << " to bytes: " << ebus::toString(bh) << std::endl
            << std::endl;
}

void checkContains() {
  const std::vector<uint8_t> search = {0x07, 0x04, 0x00};
  const std::vector<uint8_t> vec =
      ebus::toVector("3005070400be07040063030021875017d00");

  ebus::contains(vec, search) ? std::cout << "contains" << std::endl
                              : std::cout << "not contains" << std::endl;

  ebus::matches(vec, search, 2)
      ? std::cout << "contains at index 2" << std::endl
      : std::cout << "not contains at index 2" << std::endl;

  ebus::matches(vec, search, 4)
      ? std::cout << "contains at index 4" << std::endl
      : std::cout << "not contains at index 4" << std::endl;

  ebus::matches(vec, search, 6)
      ? std::cout << "contains at index 6" << std::endl
      : std::cout << "not contains at index 6" << std::endl;
}

int main() {
  printSequence("ff52b509030d0600");

  printMasterSlave("0004070400", "0ab5504d53303001074302");

  createTelegram("1008b5110203001e000a0e028709b104032c00007e00");

  checkTargetMasterSlave();

  printSequence("3010b504020d00");

  printSequence("01feb5050427002d00");

  printSequence("1050b5040101");

  printSequence("091403000000fe000100b9");

  createTelegram("1008b5130304cd017f000acd01000000000100010000");

  printSequence("0acd010000000001000100");

  printSequence("7705b509010a");

  printEncodeDecode(0x1, 0xb5);

  printDecodeEncode(0xc6, 0xb5);

  printEncodeDecode(0x1, 0x11);

  printDecodeEncode(0xc4, 0x91);

  printEncodeDecode(0x1, 0xfc);

  printDecodeEncode(0xc7, 0xbc);

  printEncodeDecode(0xc7, 0xbe);

  printDecodeEncode(0xc7, 0xbe);

  printFloatTestLittleEndian();

  printFloatTestBigEndian();

  checkContains();

  printEncodeDecode(0x02, 0x31);

  printEncodeDecode(0x02, 0xff);

  return EXIT_SUCCESS;
}
