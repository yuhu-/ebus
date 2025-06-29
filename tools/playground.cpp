/*
 * Copyright (C) 2012-2025 Roland Jax
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

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "Datatypes.hpp"
#include "Sequence.hpp"
#include "Telegram.hpp"

void printSequence(const std::string &strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::to_vector(strSequence));

  std::cout << "sequence: " << sequence.to_string() << std::endl;
  std::cout << "     crc: " << ebus::to_string(sequence.crc()) << std::endl
            << std::endl;
}

void printMasterSlave(const std::string &strMaster,
                      const std::string &strSlave) {
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

void createTelegram(const std::string &strSequence) {
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

int main() {
  printSequence("ff52b509030d0600");

  printMasterSlave("0004070400", "0ab5504d53303001074302");

  createTelegram("1008b5110203001e000a0e028709b104032c00007e00");

  // checkTargetMasterSlave();

  printSequence("3010b504020d00");

  printSequence("01feb5050427002d00");

  printSequence("1050b5040101");

  printSequence("091403000000fe000100b9");

  return EXIT_SUCCESS;
}
