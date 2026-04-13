/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/telegram.hpp"

#include <sstream>

ebus::TelegramType ebus::typeOf(uint8_t byte) {
  if (byte == sym_broad)
    return TelegramType::broadcast;
  else if (isMaster(byte))
    return TelegramType::master_master;
  else
    return TelegramType::master_slave;
}

ebus::Telegram::Telegram(Sequence& sequence) { parse(sequence); }

void ebus::Telegram::parse(Sequence& sequence) {
  clear();
  sequence.reduce();
  size_t offset = 0;

  // 1. Parse Master Part (Handles ZZ QQ PB SB NN + Data + CRC + ACK)
  if (!parseSequencePart(sequence, offset, true)) return;

  // Handle first NAK from slave (retry logic)
  if (telegram_type_ != TelegramType::broadcast && master_ack_ == sym_nak) {
    offset += master_.size() + 2;  // skip payload + CRC + ACK
    master_.clear();
    if (!parseSequencePart(sequence, offset, true)) return;
    if (master_ack_ == sym_nak) {
      master_state_ = SequenceState::err_ack_negative;
      return;
    }
  }

  // 2. Parse Slave Part if applicable
  if (telegram_type_ == TelegramType::master_slave) {
    offset += 5 + master_nn_ + 2;
    if (!parseSequencePart(sequence, offset, false)) return;

    // Handle first NAK from master
    if (slave_ack_ == sym_nak) {
      offset += slave_.size() + 2;  // skip payload + CRC + ACK
      slave_.clear();
      if (!parseSequencePart(sequence, offset, false)) return;
      if (slave_ack_ == sym_nak) {
        slave_state_ = SequenceState::err_ack_negative;
      }
    }
  }
}

void ebus::Telegram::createMaster(uint8_t source_address,
                                  const std::vector<uint8_t>& data) {
  Sequence sequence;
  sequence.pushBack(source_address, false);
  for (uint8_t b : data) sequence.pushBack(b, false);
  createMaster(sequence);
}

void ebus::Telegram::createMaster(Sequence& sequence) {
  master_state_ = SequenceState::seq_ok;
  sequence.reduce();

  // sequence is too short
  if (sequence.size() < 5) {
    master_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // source address is invalid
  if (!isMaster(sequence[0])) {
    master_state_ = SequenceState::err_source_address;
    return;
  }

  // target address is invalid
  if (!isTarget(sequence[1])) {
    master_state_ = SequenceState::err_target_address;
    return;
  }

  // data byte is invalid
  if (uint8_t(sequence[4]) > max_bytes) {
    master_state_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (sequence.size() < static_cast<size_t>(5 + uint8_t(sequence[4]))) {
    master_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (sequence.size() > static_cast<size_t>(5 + uint8_t(sequence[4]) + 1)) {
    master_state_ = SequenceState::err_seq_too_long;
    return;
  }

  telegram_type_ = typeOf(sequence[1]);
  master_nn_ = static_cast<size_t>(uint8_t(sequence[4]));

  if (sequence.size() == static_cast<size_t>(5 + master_nn_)) {
    master_.assign(sequence, 0);
    master_crc_ = sequence.crc();
  } else {
    master_.assign(sequence, 0, 5 + master_nn_);
    master_crc_ = sequence[5 + master_nn_];

    // CRC byte is invalid
    if (master_.crc() != master_crc_)
      master_state_ = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::createSlave(const std::vector<uint8_t>& data) {
  Sequence sequence;
  for (uint8_t b : data) sequence.pushBack(b, false);
  createSlave(sequence);
}

void ebus::Telegram::createSlave(Sequence& sequence) {
  slave_state_ = SequenceState::seq_ok;
  sequence.reduce();

  // sequence is too short
  if (sequence.size() < static_cast<size_t>(2)) {
    slave_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // data byte is invalid
  if (uint8_t(sequence[0]) > max_bytes) {
    slave_state_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (sequence.size() < static_cast<size_t>(1 + uint8_t(sequence[0]))) {
    slave_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (sequence.size() > static_cast<size_t>(1 + uint8_t(sequence[0]) + 1)) {
    slave_state_ = SequenceState::err_seq_too_long;
    return;
  }

  slave_nn_ = static_cast<size_t>(uint8_t(sequence[0]));

  if (sequence.size() == (1 + slave_nn_)) {
    slave_.assign(sequence, 0);
    slave_crc_ = sequence.crc();
  } else {
    slave_.assign(sequence, 0, 1 + slave_nn_);
    slave_crc_ = sequence[1 + slave_nn_];

    // CRC byte is invalid
    if (slave_.crc() != slave_crc_)
      slave_state_ = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::clear() {
  telegram_type_ = TelegramType::undefined;

  master_.clear();
  master_nn_ = 0;
  master_crc_ = sym_zero;
  master_ack_ = sym_zero;
  master_state_ = SequenceState::seq_empty;

  slave_.clear();
  slave_nn_ = 0;
  slave_crc_ = sym_zero;
  slave_ack_ = sym_zero;
  slave_state_ = SequenceState::seq_empty;
}

const ebus::Sequence& ebus::Telegram::getMaster() const { return master_; }

uint8_t ebus::Telegram::getSourceAddress() const { return master_[0]; }

uint8_t ebus::Telegram::getTargetAddress() const { return master_[1]; }

uint8_t ebus::Telegram::getPrimaryCommand() const { return master_[2]; }

uint8_t ebus::Telegram::getSecondaryCommand() const { return master_[3]; }

uint8_t ebus::Telegram::getMasterNumberBytes() const { return master_[4]; }

std::vector<uint8_t> ebus::Telegram::getMasterDataBytes() const {
  return master_.range(5, master_.size() - 5);
}

uint8_t ebus::Telegram::getMasterCRC() const { return master_crc_; }

ebus::SequenceState ebus::Telegram::getMasterState() const {
  return master_state_;
}

void ebus::Telegram::setMasterACK(uint8_t ack_byte) { master_ack_ = ack_byte; }

uint8_t ebus::Telegram::getMasterACK() const { return master_ack_; }

const ebus::Sequence& ebus::Telegram::getSlave() const { return slave_; }

uint8_t ebus::Telegram::getSlaveNumberBytes() const { return slave_[0]; }

std::vector<uint8_t> ebus::Telegram::getSlaveDataBytes() const {
  return slave_.range(1, slave_.size() - 1);
}

uint8_t ebus::Telegram::getSlaveCRC() const { return slave_crc_; }

ebus::SequenceState ebus::Telegram::getSlaveState() const {
  return slave_state_;
}

void ebus::Telegram::setSlaveACK(uint8_t ack_byte) { slave_ack_ = ack_byte; }

uint8_t ebus::Telegram::getSlaveACK() const { return slave_ack_; }

ebus::TelegramType ebus::Telegram::getType() const { return telegram_type_; }

bool ebus::Telegram::isValid() const {
  if (telegram_type_ != TelegramType::master_slave)
    return master_state_ == SequenceState::seq_ok;

  return (master_state_ == SequenceState::seq_ok &&
          slave_state_ == SequenceState::seq_ok);
}

std::string ebus::Telegram::toString() const {
  std::ostringstream ostr;

  ostr << toStringMaster();

  if (master_state_ == SequenceState::seq_ok &&
      telegram_type_ == TelegramType::master_slave)
    ostr << " " << toStringSlave();

  return ostr.str();
}

std::string ebus::Telegram::toStringMaster() const {
  std::ostringstream ostr;
  if (master_state_ != SequenceState::seq_ok)
    ostr << toStringMasterState();
  else
    ostr << master_.toString();

  return ostr.str();
}

std::string ebus::Telegram::toStringSlave() const {
  std::ostringstream ostr;
  if (slave_state_ != SequenceState::seq_ok &&
      telegram_type_ != TelegramType::broadcast) {
    ostr << toStringSlaveState();
  } else {
    ostr << slave_.toString();
  }

  return ostr.str();
}

std::string ebus::Telegram::toStringMasterState() const {
  std::ostringstream ostr;
  if (master_.size() > 0) ostr << "'" << master_.toString() << "' ";

  ostr << "master " << ebus::getSequenceStateText(master_state_);

  return ostr.str();
}

std::string ebus::Telegram::toStringSlaveState() const {
  std::ostringstream ostr;
  if (slave_.size() > 0) ostr << "'" << slave_.toString() << "' ";

  ostr << "slave " << getSequenceStateText(slave_state_);

  return ostr.str();
}

ebus::SequenceState ebus::Telegram::checkMasterSequence(
    const Sequence& sequence, size_t offset) {
  // sequence is too short
  if (sequence.size() < static_cast<size_t>(5))
    return SequenceState::err_seq_too_short;

  // source address is invalid
  if (!isMaster(sequence[offset + 0])) return SequenceState::err_source_address;

  // target address is invalid
  if (!isTarget(sequence[offset + 1])) return SequenceState::err_target_address;

  // data byte is invalid
  if (uint8_t(sequence[offset + 4]) > max_bytes)
    return SequenceState::err_data_byte;

  // sequence is too short (incl. CRC)
  if (sequence.size() <
      static_cast<size_t>(5 + uint8_t(sequence[offset + 4]) + 1))
    return SequenceState::err_seq_too_short;

  return SequenceState::seq_ok;
}

ebus::SequenceState ebus::Telegram::checkSlaveSequence(const Sequence& sequence,
                                                       size_t offset) {
  // sequence is too short
  if (sequence.size() < static_cast<size_t>(1))
    return SequenceState::err_seq_too_short;

  // data byte is invalid
  if (uint8_t(sequence[offset + 0]) > max_bytes)
    return SequenceState::err_data_byte;

  // sequence is too short (incl. CRC)
  if (sequence.size() <
      static_cast<size_t>(1 + uint8_t(sequence[offset + 0]) + 1))
    return SequenceState::err_seq_too_short;

  return SequenceState::seq_ok;
}

bool ebus::Telegram::parseSequencePart(const Sequence& sequence, size_t& offset,
                                       bool is_master) {
  if (is_master) {
    master_state_ = checkMasterSequence(sequence, offset);
    if (master_state_ != SequenceState::seq_ok) return false;
    master_.assign(sequence, offset,
                   5 + static_cast<size_t>(sequence[offset + 4]) + 1);
    createMaster(master_);
    if (master_state_ != SequenceState::seq_ok) return false;

    if (telegram_type_ != TelegramType::broadcast) {
      if (sequence.size() <= offset + 5 + master_nn_ + 1) {
        master_state_ = SequenceState::err_ack_missing;
        return false;
      }
      master_ack_ = sequence[offset + 5 + master_nn_ + 1];
      if (master_ack_ != sym_ack && master_ack_ != sym_nak) {
        master_state_ = SequenceState::err_ack_invalid;
        return false;
      }
    }
  } else {
    slave_state_ = checkSlaveSequence(sequence, offset);
    if (slave_state_ != SequenceState::seq_ok) return false;
    slave_.assign(sequence, offset,
                  1 + static_cast<size_t>(sequence[offset + 0]) + 1);
    createSlave(slave_);
    if (slave_state_ != SequenceState::seq_ok) return false;

    if (sequence.size() <= offset + 1 + slave_nn_ + 1) {
      slave_state_ = SequenceState::err_ack_missing;
      return false;
    }
    slave_ack_ = sequence[offset + 1 + slave_nn_ + 1];
    if (slave_ack_ != sym_ack && slave_ack_ != sym_nak) {
      slave_state_ = SequenceState::err_ack_invalid;
      return false;
    }
  }
  return true;
}
