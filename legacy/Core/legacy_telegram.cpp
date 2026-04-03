/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

#include "Core/Sequence.hpp"
#include "Core/Telegram.hpp"
#include "Utils/Common.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_creation() {
  std::cout << "\n=== Test: Telegram Creation ===" << std::endl;
  ebus::Telegram tel;

  // Create Master
  // Data: 10 (Src) 08 (Dst) b5 09 (Cmd) 03 (Len) 0d 06 00 (Payload)
  // Expected CRC: 0xe1
  tel.createMaster(0x10, ebus::to_vector("08b509030d0600"));
  run_test("Create Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Create Master Source", tel.getSourceAddress() == 0x10);
  run_test("Create Master Target", tel.getTargetAddress() == 0x08);
  run_test("Create Master CRC", tel.getMasterCRC() == 0xe1);

  // Create Slave
  // Data: 03 (Len) b0 fb aa (Payload)
  // Expected CRC: 0xd0
  tel.createSlave(ebus::to_vector("03b0fbaa"));
  run_test("Create Slave State OK",
           tel.getSlaveState() == ebus::SequenceState::seq_ok);
  run_test("Create Slave Num Bytes", tel.getSlaveNumberBytes() == 0x03);
  run_test("Create Slave Data",
           tel.getSlaveDataBytes() == ebus::to_vector("b0fbaa"));
  run_test("Create Slave CRC", tel.getSlaveCRC() == 0xd0);
}

void test_parsing() {
  std::cout << "\n=== Test: Telegram Parsing ===" << std::endl;
  ebus::Sequence seq;
  ebus::Telegram tel;

  // Test 1: Normal Master-Slave
  // 10 08 b5 09 03 0d 06 00 [Master Payload]
  // e1                      [Master CRC]
  // 00                      [Master ACK]
  // 03 b0 fb aa             [Slave Payload] -> Wire: 03 b0 fb a9 01
  // d0 00                   [Slave CRC + ACK]
  seq.assign(ebus::to_vector("1008b509030d0600e10003b0fba901d000"), true);
  tel.parse(seq);
  run_test("Parse MS: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse MS: Slave State OK",
           tel.getSlaveState() == ebus::SequenceState::seq_ok);
  run_test("Parse MS: Type", tel.getType() == ebus::TelegramType::master_slave);
  run_test("Parse MS: Master ACK", tel.getMasterACK() == 0x00);
  run_test("Parse MS: Slave ACK", tel.getSlaveACK() == 0x00);
  run_test("Parse MS: Master Data",
           tel.getMasterDataBytes() == ebus::to_vector("0d0600"));
  run_test("Parse MS: Slave Data",
           tel.getSlaveDataBytes() == ebus::to_vector("b0fbaa"));

  // Test 2: Master-Master
  seq.assign(ebus::to_vector("1000b5050427002400d900"), true);
  tel.parse(seq);
  run_test("Parse MM: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse MM: Type",
           tel.getType() == ebus::TelegramType::master_master);
  run_test("Parse MM: Master ACK", tel.getMasterACK() == 0x00);
  run_test("Parse MM: Slave State Empty",
           tel.getSlaveState() == ebus::SequenceState::seq_empty);

  // Test 3: Broadcast
  seq.assign(ebus::to_vector("10fe07000970160443183105052592"), true);
  tel.parse(seq);
  run_test("Parse BC: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse BC: Type", tel.getType() == ebus::TelegramType::broadcast);
  run_test("Parse BC: Slave State Empty",
           tel.getSlaveState() == ebus::SequenceState::seq_empty);

  // Test 4: NAK from slave, then retry, NAK from master, then retry
  // Master 1: ... 8a (CRC) ff (NAK)
  // Master 2: ... 8a (CRC) 00 (ACK) -> Success
  // Slave 1:  ... a7 (CRC) ff (NAK)
  // Slave 2:  ... a7 (CRC) 00 (ACK) -> Success
  seq.assign(
      ebus::to_vector("1008b51101028aff1008b51101028a00013ca7ff013ca700"),
      true);
  tel.parse(seq);
  run_test("Parse NAK: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse NAK: Slave State OK",
           tel.getSlaveState() == ebus::SequenceState::seq_ok);
  run_test("Parse NAK: Master Data (final)",
           tel.getMasterDataBytes() == ebus::to_vector("02"));
  run_test("Parse NAK: Slave Data (final)",
           tel.getSlaveDataBytes() == ebus::to_vector("3c"));

  // Test 5: Invalid CRC
  seq.assign(ebus::to_vector("1008b509030d06009f"), true);  // Correct CRC is e1
  tel.parse(seq);
  run_test("Parse Invalid CRC",
           tel.getMasterState() == ebus::SequenceState::err_crc_invalid);

  // Test 6: Extended Bytes (A9, AA)
  // Master: 10 08 b5 04 02 a9 aa (Logical)
  // Wire:   10 08 b5 04 02 a9 00 a9 01
  // Slave:  ... empty ...

  // Calculate CRC for verification
  ebus::Sequence tempSeq;
  tempSeq.assign(ebus::to_vector("1008b50402a9aa"), false);
  uint8_t expectedCrc = tempSeq.crc();

  std::string wireStr =
      "1008b50402a900a901" + ebus::to_string(expectedCrc) + "00";
  seq.assign(ebus::to_vector(wireStr), true);
  tel.parse(seq);
  run_test("Parse Ext: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse Ext: Master Data",
           tel.getMasterDataBytes() == ebus::to_vector("a9aa"));

  // Test 7: Complex Retry (Master NAK/Retry/ACK, Slave NAK/Retry/ACK)
  // Master: ff 52 b5 09 03 0d 06 a9 (Logical)
  // Slave:  03 b0 fb aa (Logical)

  // Calculate Master CRC dynamically for the new payload
  ebus::Sequence masterSeqRetry;
  masterSeqRetry.assign(ebus::to_vector("ff52b509030d06a9"), false);
  uint8_t masterCrcRetry = masterSeqRetry.crc();
  std::string masterCrcHex = ebus::to_string(masterCrcRetry);

  // Wire: ff 52 b5 09 03 0d 06 a9 00 [CRC] ...
  std::string masterWire = "ff52b509030d06a900";
  std::string slaveWirePart = "03b0fba901d0";  // d0 is known CRC for 03b0fbaa

  std::string complexSeq = masterWire + masterCrcHex + "ff" + masterWire +
                           masterCrcHex + "00" + slaveWirePart + "ff" +
                           slaveWirePart + "00";

  seq.assign(ebus::to_vector(complexSeq), true);
  tel.parse(seq);
  run_test("Parse Complex Retry: Master State OK",
           tel.getMasterState() == ebus::SequenceState::seq_ok);
  run_test("Parse Complex Retry: Slave State OK",
           tel.getSlaveState() == ebus::SequenceState::seq_ok);
  run_test("Parse Complex Retry: Master Data",
           tel.getMasterDataBytes() == ebus::to_vector("0d06a9"));
  run_test("Parse Complex Retry: Slave Data",
           tel.getSlaveDataBytes() == ebus::to_vector("b0fbaa"));
}

int main() {
  test_creation();
  test_parsing();

  std::cout << "\nAll telegram tests passed!" << std::endl;

  return EXIT_SUCCESS;
}
