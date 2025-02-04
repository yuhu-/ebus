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

#include "Sequence.h"
#include "Telegram.h"

void printSequence(const std::string &strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::Sequence::to_vector(strSequence));

  std::cout << "sequence: " << sequence.to_string() << std::endl;
  std::cout << "     crc: " << ebus::Sequence::to_string(sequence.crc())
            << std::endl
            << std::endl;
}

void printMasterSlave(const std::string &strMaster,
                      const std::string &strSlave) {
  ebus::Sequence master;
  master.assign(ebus::Sequence::to_vector(strMaster));

  ebus::Sequence slave;
  slave.assign(ebus::Sequence::to_vector(strSlave));

  ebus::Telegram tel;
  tel.createMaster(master);
  tel.createSlave(slave);

  std::cout << "  master: " << tel.toStringMaster() << std::endl;
  std::cout << "     crc: " << ebus::Sequence::to_string(tel.getMasterCRC())
            << std::endl;
  std::cout << "   slave: " << tel.toStringSlave() << std::endl;
  std::cout << "     crc: " << ebus::Sequence::to_string(tel.getSlaveCRC())
            << std::endl
            << std::endl;
}

void createTelegram(const std::string &strSequence) {
  ebus::Sequence sequence;
  sequence.assign(ebus::Sequence::to_vector(strSequence));

  ebus::Telegram tel(sequence);

  std::ostringstream ostr;
  ostr << "telegram: " << sequence.to_string() << std::endl;

  ostr << "telegram: " << tel.getMaster().to_string() << "    "
       << tel.getSlave().to_string() << std::endl;

  std::cout << ostr.str() << std::endl;
}

int main() {
  printSequence("ff52b509030d0600");

  printMasterSlave("0004070400", "0ab5504d53303001074302");

  createTelegram("1008b5110203001e000a0e028709b104032c00007e00");

  return (0);
}
