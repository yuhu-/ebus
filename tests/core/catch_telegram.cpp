/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>

#include "core/sequence.hpp"
#include "core/telegram.hpp"
#include "utils/common.hpp"

TEST_CASE("Telegram: creation", "[core][telegram]") {
  ebus::Telegram tel;

  SECTION("Create Master") {
    tel.createMaster(0x10, ebus::toVector("08b509030d0600"));
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getSourceAddress() == 0x10);
    REQUIRE(tel.getTargetAddress() == 0x08);
    REQUIRE(tel.getMasterCRC() == 0xe1);
  }

  SECTION("Create Slave") {
    tel.createSlave(ebus::toVector("03b0fbaa"));
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getSlaveNumberBytes() == 0x03);
    REQUIRE(tel.getSlaveDataBytes() == ebus::toVector("b0fbaa"));
    REQUIRE(tel.getSlaveCRC() == 0xd0);
  }
}

TEST_CASE("Telegram: parsing", "[core][telegram]") {
  ebus::Sequence seq;
  ebus::Telegram tel;

  SECTION("Master-Slave normal") {
    seq.assign(ebus::toVector("1008b509030d0600e10003b0fba901d000"), true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getType() == ebus::TelegramType::master_slave);
    REQUIRE(tel.getMasterACK() == 0x00);
    REQUIRE(tel.getSlaveACK() == 0x00);
    REQUIRE(tel.getMasterDataBytes() == ebus::toVector("0d0600"));
    REQUIRE(tel.getSlaveDataBytes() == ebus::toVector("b0fbaa"));
  }

  SECTION("Master-Master") {
    seq.assign(ebus::toVector("1000b5050427002400d900"), true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getType() == ebus::TelegramType::master_master);
    REQUIRE(tel.getMasterACK() == 0x00);
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_empty);
  }

  SECTION("Broadcast") {
    seq.assign(ebus::toVector("10fe07000970160443183105052592"), true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getType() == ebus::TelegramType::broadcast);
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_empty);
  }

  SECTION("NAK/Retry sequence") {
    seq.assign(
        ebus::toVector("1008b51101028aff1008b51101028a00013ca7ff013ca700"),
        true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getMasterDataBytes() == ebus::toVector("02"));
    REQUIRE(tel.getSlaveDataBytes() == ebus::toVector("3c"));
  }

  SECTION("Invalid CRC") {
    seq.assign(ebus::toVector("1008b509030d06009f"),
               true);  // correct CRC is 0xe1
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::err_crc_invalid);
  }

  SECTION("Extended bytes (A9/AA) and CRC calculation") {
    ebus::Sequence tmpSeq;
    tmpSeq.assign(ebus::toVector("1008b50402a9aa"), false);
    uint8_t expectedCrc = tmpSeq.crc();

    std::string wireStr =
        "1008b50402a900a901" + ebus::toString(expectedCrc) + "00";
    seq.assign(ebus::toVector(wireStr), true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getMasterDataBytes() == ebus::toVector("a9aa"));
  }

  SECTION("Complex retry (master + slave retries)") {
    // Build master retry pattern similar to old test: master attempts, NAK,
    // retry -> ACK; slave attempts NAK then ACK
    ebus::Sequence masterSeqRetry;
    masterSeqRetry.assign(ebus::toVector("ff52b509030d06a9"), false);
    uint8_t masterCrcRetry = masterSeqRetry.crc();
    std::string masterCrcHex = ebus::toString(masterCrcRetry);

    std::string masterWire = "ff52b509030d06a900";
    std::string slaveWirePart = "03b0fba901d0";  // known CRC d0 for 03b0fbaa

    std::string complexSeq = masterWire + masterCrcHex + "ff" + masterWire +
                             masterCrcHex + "00" + slaveWirePart + "ff" +
                             slaveWirePart + "00";

    seq.assign(ebus::toVector(complexSeq), true);
    tel.parse(seq);
    REQUIRE(tel.getMasterState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getSlaveState() == ebus::SequenceState::seq_ok);
    REQUIRE(tel.getMasterDataBytes() == ebus::toVector("0d06a9"));
    REQUIRE(tel.getSlaveDataBytes() == ebus::toVector("b0fbaa"));
  }
}