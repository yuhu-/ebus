/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <ebus/definitions.hpp>
#include <string>
#include <vector>

#include "core/sequence.hpp"
#include "utils/common.hpp"

namespace ebus {

enum class SequenceState {
  seq_empty,           // sequence is empty
  seq_ok,              // sequence is ok
  err_seq_too_short,   // sequence is too short
  err_seq_too_long,    // sequence is too long
  err_source_address,  // source address is invalid
  err_target_address,  // target address is invalid
  err_data_byte,       // data byte is invalid
  err_crc_invalid,     // CRC byte is invalid
  err_ack_invalid,     // acknowledge byte is invalid
  err_ack_missing,     // acknowledge byte is missing
  err_ack_negative     // acknowledge byte is negative
};

static const char* getSequenceStateText(SequenceState state) {
  const char* values[] = {
      "sequence is empty",           "sequence is ok",
      "sequence is too short",       "sequence is too long",
      "source address is invalid",   "target address is invalid",
      "data byte is invalid",        "CRC byte is invalid",
      "acknowledge byte is invalid", "acknowledge byte is missing",
      "acknowledge byte is negative"};
  return values[static_cast<int>(state)];
}

ebus::TelegramType typeOf(const uint8_t byte);

/**
 * Based on the eBUS specification, the Telegram class can parse, create, and
 * evaluate sequences of bytes that represent Master-Slave, Master-Master, and
 * Broadcast telegrams. It provides methods to extract the source and target
 * addresses, primary and secondary commands, data bytes, and CRC. The class
 * also checks the validity of the telegrams according to the specification and
 * can convert them to string representations.
 *
 * Master-Slave Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC                ACK SYN
 * Receiver ( Slave):                        ACK NN DBx CRC
 *
 * Master-Master Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC     SYN
 * Receiver (Master):                        ACK
 *
 * Broadcast Telegram:
 * Sender   (Master): QQ ZZ PB SB NN DBx CRC SYN
 * Receiver    (All):
 *
 * QQ...Source address (25 possible addresses)
 * ZZ...Target address (254 possible addresses)
 * PB...Primary command
 * SB...Secondary command
 * NN...Number of data bytes (0 <= NN <= 16)
 * DBx..Data bytes (payload)
 * CRC..8-Bit CRC byte
 * ACK..Acknowledgement byte (0x00 OK, 0xff NOK)
 * SYN..Synchronisation byte (0xaa)
 */
class Telegram {
 public:
  Telegram() = default;
  explicit Telegram(Sequence& seq);

  void parse(Sequence& seq);

  void createMaster(const uint8_t src, const std::vector<uint8_t>& vec);
  void createMaster(Sequence& seq);

  void createSlave(const std::vector<uint8_t>& vec);
  void createSlave(Sequence& seq);

  void clear();

  // returns the master sequence [QQ ZZ PB SB NN DBx] without CRC byte
  const Sequence& getMaster() const;

  uint8_t getSourceAddress() const;
  uint8_t getTargetAddress() const;

  uint8_t getPrimaryCommand() const;
  uint8_t getSecondaryCommand() const;

  uint8_t getMasterNumberBytes() const;
  const std::vector<uint8_t> getMasterDataBytes() const;

  uint8_t getMasterCRC() const;
  ebus::SequenceState getMasterState() const;

  void setMasterACK(const uint8_t byte);
  uint8_t getMasterACK() const;

  // returns the slave sequence [NN DBx] without CRC byte
  const Sequence& getSlave() const;

  uint8_t getSlaveNumberBytes() const;
  const std::vector<uint8_t> getSlaveDataBytes() const;

  uint8_t getSlaveCRC() const;
  ebus::SequenceState getSlaveState() const;

  void setSlaveACK(const uint8_t byte);
  uint8_t getSlaveACK() const;

  ebus::TelegramType getType() const;

  bool isValid() const;

  const std::string toString() const;
  const std::string toStringMaster() const;
  const std::string toStringSlave() const;

  const std::string toStringMasterState() const;
  const std::string toStringSlaveState() const;

  static ebus::SequenceState checkMasterSequence(const Sequence& seq);
  static ebus::SequenceState checkSlaveSequence(const Sequence& seq);

 private:
  TelegramType telegram_type_ = TelegramType::undefined;

  Sequence master_;
  size_t master_nn_ = 0;
  uint8_t master_crc_ = sym_zero;
  uint8_t master_ack_ = sym_zero;
  SequenceState master_state_ = SequenceState::seq_empty;

  Sequence slave_;
  size_t slave_nn_ = 0;
  uint8_t slave_crc_ = sym_zero;
  uint8_t slave_ack_ = sym_zero;
  SequenceState slave_state_ = SequenceState::seq_empty;
};

}  // namespace ebus
