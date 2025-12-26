/*
 * Copyright (C) 2023-2025 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "Datatypes.hpp"
#include "Sequence.hpp"
#include "Telegram.hpp"

void printSequence(const std::string& strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::to_vector(strSequence));

  std::cout << "sequence: " << sequence.to_string() << std::endl;
  std::cout << "     crc: " << ebus::to_string(sequence.crc()) << std::endl
            << std::endl;
}

void printMasterSlave(const std::string& strMaster,
                      const std::string& strSlave) {
  ebus::Sequence master;
  master.assign(ebus::to_vector(strMaster));

  ebus::Sequence slave;
  slave.assign(ebus::to_vector(strSlave));

  ebus::Telegram tel;
  tel.createMaster(master);
  tel.createSlave(slave);

  std::cout << "  master: " << tel.toStringMaster() << std::endl;
  std::cout << "     crc: " << ebus::to_string(tel.getMasterCRC()) << std::endl;
  std::cout << "   slave: " << tel.toStringSlave() << std::endl;
  std::cout << "     crc: " << ebus::to_string(tel.getSlaveCRC()) << std::endl
            << std::endl;
}

void createTelegram(const std::string& strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::to_vector(strSequence));

  ebus::Telegram tel(sequence);

  std::ostringstream ostr;
  ostr << "telegram: " << sequence.to_string() << std::endl;

  ostr << "telegram: " << tel.getMaster().to_string() << "    "
       << tel.getSlave().to_string() << std::endl;

  std::cout << ostr.str() << std::endl;
}

void checkTargetMasterSlave() {
  for (int b0 = 0x00; b0 <= 0xff; b0++) {
    std::cout << "i: 0x" << ebus::to_string(b0);

    if (ebus::isTarget(b0)) std::cout << " isTarget";

    if (ebus::isMaster(b0)) std::cout << " isMaster";

    if (ebus::isSlave(b0)) std::cout << " isSlave";

    if (b0 != ebus::slaveOf(b0))
      std::cout << " ==> slaveOf  = 0x" << ebus::to_string(ebus::slaveOf(b0));

    if (b0 != ebus::masterOf(b0))
      std::cout << "  ==> masterOf = 0x" << ebus::to_string(ebus::masterOf(b0));

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
  std::cout << "Original: 0x" << ebus::to_string(c) << " 0x"
            << ebus::to_string(d) << std::endl;

  uint8_t decoded[2];
  decode(c, d, decoded);
  std::cout << "Decoded:  0x" << ebus::to_string(decoded[0]) << " 0x"
            << ebus::to_string(decoded[1]) << std::endl;

  uint8_t encoded[2];
  encode(decoded[0], decoded[1], encoded);
  std::cout << "Encoded:  0x" << ebus::to_string(encoded[0]) << " 0x"
            << ebus::to_string(encoded[1]) << std::endl
            << std::endl;
}

void printEncodeDecode(uint8_t c, uint8_t d) {
  std::cout << "Original: 0x" << ebus::to_string(c) << " 0x"
            << ebus::to_string(d) << std::endl;

  uint8_t encoded[2];
  encode(c, d, encoded);
  std::cout << "Encoded:  0x" << ebus::to_string(encoded[0]) << " 0x"
            << ebus::to_string(encoded[1]) << std::endl;

  uint8_t decoded[2];
  decode(encoded[0], encoded[1], decoded);
  std::cout << "Decoded:  0x" << ebus::to_string(decoded[0]) << " 0x"
            << ebus::to_string(decoded[1]) << std::endl
            << std::endl;
}

void printFloatTest() {
  float f = 21.0375f;
  std::vector<uint8_t> bytes = ebus::float_2_byte(static_cast<double_t>(f));
  std::cout << "float: " << f << " to bytes: " << ebus::to_string(bytes)
            << std::endl;
  double_t df = ebus::byte_2_float(bytes);
  std::cout << "bytes: " << ebus::to_string(bytes) << " to float: " << df
            << std::endl
            << std::endl;

  std::vector<uint8_t> h = {0xcd, 0x4c, 0xb2, 0x41};
  double_t dh = ebus::byte_2_float(h);
  std::cout << "bytes: " << ebus::to_string(h) << " to float: " << dh
            << std::endl;
  std::vector<uint8_t> bh = ebus::float_2_byte(dh);
  std::cout << "float: " << dh << " to bytes: " << ebus::to_string(bh)
            << std::endl
            << std::endl;
}

void checkContains() {
  const std::vector<uint8_t> search = {0x07, 0x04, 0x00};
  const std::vector<uint8_t> vec =
      ebus::to_vector("3005070400be07040063030021875017d00");

  ebus::contains(vec, search) ? std::cout << "contains" << std::endl
                              : std::cout << "not contains" << std::endl;

  ebus::contains(vec, search, 2)
      ? std::cout << "contains at index 2" << std::endl
      : std::cout << "not contains at index 2" << std::endl;

  ebus::contains(vec, search, 4)
      ? std::cout << "contains at index 4" << std::endl
      : std::cout << "not contains at index 4" << std::endl;

  ebus::contains(vec, search, 6)
      ? std::cout << "contains at index 6" << std::endl
      : std::cout << "not contains at index 6" << std::endl;
}

int main() {
  printSequence("ff52b509030d0600");

  printMasterSlave("0004070400", "0ab5504d53303001074302");

  createTelegram("1008b5110203001e000a0e028709b104032c00007e00");

  // checkTargetMasterSlave();

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

  printEncodeDecode(0x2, 0x77);

  printDecodeEncode(0xc9, 0xb7);

  printFloatTest();

  checkContains();

  return EXIT_SUCCESS;
}
