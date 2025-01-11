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
#include <iostream>
#include <string>

#include "Sequence.h"
#include "Telegram.h"

int main() {
  ebus::Sequence seq;

  // parse sequence
  seq.assign(ebus::Sequence::to_vector("ff12b509030d0000d700037702006100"));

  ebus::Telegram parse(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  range: " << parse.to_string() << " slave(1,2) = '"
            << ebus::Sequence::to_string(parse.getSlave().range(1, 2)) << "'"
            << std::endl
            << std::endl;

  // parse sequence
  seq.assign(ebus::Sequence::to_vector("ff0ab509030d0e00830002e0028900"));

  ebus::Telegram parse2(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  range: " << parse2.to_string() << " slave(1,2) = '"
            << ebus::Sequence::to_string(parse2.getSlave().range(1, 2)) << "'"
            << std::endl
            << std::endl;

  // create telegram
  ebus::Telegram tel;
  tel.createMaster(uint8_t(0xff), ebus::Sequence::to_vector("52b509030d0600"));
  std::cout << "    seq: ff52b509030d0600" << std::endl;
  std::cout << " master: " << tel.toStringMaster() << std::endl << std::endl;

  seq.assign(ebus::Sequence::to_vector("ff52b509030d060043"));

  tel.createMaster(seq);
  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << " master: " << tel.toStringMaster() << std::endl << std::endl;

  // create slave
  tel.createSlave(ebus::Sequence::to_vector("03b0fbaa"));
  std::cout << "    seq: 03b0fbaa" << std::endl;
  std::cout << "  slave: " << tel.toStringSlave() << std::endl << std::endl;

  seq.assign(ebus::Sequence::to_vector("03b0fba901d0"));

  tel.createSlave(seq);
  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  slave: " << tel.toStringSlave() << std::endl << std::endl;

  // Normal
  seq.assign(ebus::Sequence::to_vector("ff52b509030d0600430003b0fba901d000"));

  ebus::Telegram full(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "   full: " << full.to_string() << " ==> Normal" << std::endl
            << std::endl;

  // NAK from slave
  seq.assign(ebus::Sequence::to_vector(
      "ff52b509030d060043ffff52b509030d0600430003b0fba901d000"));

  ebus::Telegram full2(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  full2: " << full2.to_string() << " ==> NAK from slave"
            << std::endl
            << std::endl;

  // twice NAK from slave
  seq.assign(
      ebus::Sequence::to_vector("ff52b509030d060043ffff52b509030d060043ff"));

  ebus::Telegram full22(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << " full22: " << full22.to_string() << " ==> twice NAK from slave"
            << std::endl
            << std::endl;

  // NAK from master
  seq.assign(ebus::Sequence::to_vector(
      "ff52b509030d0600430003b0fba901d0ff0003b0fba901d000"));

  ebus::Telegram full3(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  full3: " << full3.to_string() << " ==> NAK from master"
            << std::endl
            << std::endl;

  // NAK from slave and master
  seq.assign(
      ebus::Sequence::to_vector("ff52b509030d060043ffff52b509030d0600430003b0fb"
                                "a901d0ff0003b0fba901d000"));

  ebus::Telegram full4(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  full4:  " << full4.to_string()
            << " ==> NAK from slave and master" << std::endl
            << std::endl;

  // twice NAK from slave and master
  seq.assign(
      ebus::Sequence::to_vector("ff52b509030d060043ffff52b509030d0600430003b0fb"
                                "a901d0ff0003b0fba901d0ff"));

  ebus::Telegram full44(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << " full44: " << full44.to_string()
            << " ==> twice NAK from slave and master" << std::endl
            << std::endl;

  // defect sequence
  seq.assign(ebus::Sequence::to_vector("107fc2b5100900024000000000000215"));

  ebus::Telegram full5(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  full5: " << full5.to_string() << std::endl << std::endl;

  // missing acknowledge byte
  seq.assign(ebus::Sequence::to_vector(
      "1008b51101028aff1008b51101028a0003b0fba901d0ff0003b0fba901d0"));

  ebus::Telegram full6(seq);

  std::cout << "    seq: " << seq.to_string() << std::endl;
  std::cout << "  full6: " << full6.to_string() << std::endl << std::endl;

  // extend
  seq.assign(ebus::Sequence::to_vector("08b509030da900"), false);

  ebus::Telegram ext1;
  ext1.createMaster(uint8_t(0xff), seq.to_vector());

  if (ext1.getMasterState() == SEQ_OK) {
    ebus::Sequence master = ext1.getMaster();
    master.push_back(ext1.getMasterCRC(), false);
    master.extend();

    std::cout << "    seq: " << seq.to_string() << std::endl;
    std::cout << " master: " << master.to_string() << std::endl << std::endl;
  }

  seq.assign(ebus::Sequence::to_vector("08b509030daa00"), false);

  ebus::Telegram ext2;
  ext2.createMaster(uint8_t(0xff), seq.to_vector());

  if (ext2.getMasterState() == SEQ_OK) {
    ebus::Sequence master = ext2.getMaster();
    master.push_back(ext2.getMasterCRC(), false);
    master.extend();

    std::cout << "    seq: " << seq.to_string() << std::endl;
    std::cout << " master: " << master.to_string() << std::endl << std::endl;
  }

  return (0);
}
