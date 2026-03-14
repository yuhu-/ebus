/*
 * Copyright (C) 2012-2026 Roland Jax
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

#include "Telegram.hpp"

#include <sstream>

ebus::TelegramType ebus::typeOf(const uint8_t byte) {
  if (byte == sym_broad)
    return TelegramType::broadcast;
  else if (isMaster(byte))
    return TelegramType::master_master;
  else
    return TelegramType::master_slave;
}

ebus::Telegram::Telegram(Sequence& seq) { parse(seq); }

void ebus::Telegram::parse(Sequence& seq) {
  clear();
  seq.reduce();
  int offset = 0;

  masterState_ = checkMasterSequence(seq);

  if (masterState_ != SequenceState::seq_ok) return;

  Sequence masterSeq(seq, 0, 5 + uint8_t(seq[4]) + 1);
  createMaster(masterSeq);

  if (masterState_ != SequenceState::seq_ok) return;

  if (telegramType_ != TelegramType::broadcast) {
    // acknowledge byte is missing
    if (seq.size() <= static_cast<size_t>(5 + masterNN_ + 1)) {
      masterState_ = SequenceState::err_ack_missing;
      return;
    }

    masterACK_ = seq[5 + masterNN_ + 1];

    // acknowledge byte is invalid
    if (masterACK_ != sym_ack && masterACK_ != sym_nak) {
      masterState_ = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from slave
    if (masterACK_ == sym_nak) {
      // sequence is too short
      if (seq.size() < static_cast<size_t>(master_.size() + 1)) {
        masterState_ = SequenceState::err_seq_too_short;
        return;
      }

      offset = master_.size() + 1;
      master_.clear();

      Sequence tmp(seq, offset);
      masterState_ = checkMasterSequence(tmp);

      if (masterState_ != SequenceState::seq_ok) return;

      Sequence masterSeq2(tmp, 0, 5 + uint8_t(tmp[4]) + 1);
      createMaster(masterSeq2);

      if (masterState_ != SequenceState::seq_ok) return;

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(5 + masterNN_ + 1)) {
        masterState_ = SequenceState::err_ack_missing;
        return;
      }

      masterACK_ = tmp[5 + masterNN_ + 1];

      // acknowledge byte is invalid
      if (masterACK_ != sym_ack && masterACK_ != sym_nak) {
        masterState_ = SequenceState::err_ack_invalid;
        return;
      }

      // handle second NAK from slave
      if (masterACK_ == sym_nak) {
        // // sequence is too long
        // if (tmp.size() > static_cast<size_t>(5 + masterNN_ + 2))
        //   masterState_ = SequenceState::err_seq_too_long;
        // else

        // acknowledge byte is negative
        masterState_ = SequenceState::err_ack_negative;

        return;
      }
    }
  }

  if (telegramType_ == TelegramType::master_slave) {
    offset += 5 + masterNN_ + 2;

    Sequence seq2(seq, offset);
    slaveState_ = checkSlaveSequence(seq2);

    if (slaveState_ != SequenceState::seq_ok) return;

    Sequence slaveSeq(seq2, 0, 1 + uint8_t(seq2[0]) + 1);
    createSlave(slaveSeq);

    if (slaveState_ != SequenceState::seq_ok) return;

    // acknowledge byte is missing
    if (seq2.size() <= static_cast<size_t>(1 + slaveNN_ + 1)) {
      slaveState_ = SequenceState::err_ack_missing;
      return;
    }

    slaveACK_ = seq2[1 + slaveNN_ + 1];

    // acknowledge byte is invalid
    if (slaveACK_ != sym_ack && slaveACK_ != sym_nak) {
      slaveState_ = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from master
    if (slaveACK_ == sym_nak) {
      // sequence is too short
      if (seq2.size() < static_cast<size_t>(slave_.size() + 2)) {
        slaveState_ = SequenceState::err_seq_too_short;
        return;
      }

      offset = slave_.size() + 1;
      slave_.clear();

      Sequence tmp(seq2, offset);
      slaveState_ = checkSlaveSequence(tmp);

      if (slaveState_ != SequenceState::seq_ok) return;

      Sequence slaveSeq2(seq2, offset, 1 + uint8_t(seq2[offset]) + 1);
      createSlave(slaveSeq2);

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(1 + slaveNN_ + 1)) {
        slaveState_ = SequenceState::err_ack_missing;
        return;
      }

      slaveACK_ = tmp[1 + slaveNN_ + 1];

      // acknowledge byte is invalid
      if (slaveACK_ != sym_ack && slaveACK_ != sym_nak) {
        slaveState_ = SequenceState::err_ack_invalid;
        return;
      }

      // sequence is too long
      // if (tmp.size() > static_cast<size_t>(1 + slaveNN_ + 2)) {
      //   slaveState_ = SequenceState::err_seq_too_long;
      //   slave.clear();
      //   return;
      // }

      // handle second NAK from master
      if (slaveACK_ == sym_nak) {
        // acknowledge byte is negative
        slaveState_ = SequenceState::err_ack_negative;
        return;
      }
    }
  }
}

void ebus::Telegram::createMaster(const uint8_t src,
                                  const std::vector<uint8_t>& vec) {
  Sequence seq;

  seq.push_back(src, false);

  for (size_t i = 0; i < vec.size(); i++) seq.push_back(vec.at(i), false);

  createMaster(seq);
}

void ebus::Telegram::createMaster(Sequence& seq) {
  masterState_ = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < 5) {
    masterState_ = SequenceState::err_seq_too_short;
    return;
  }

  // source address is invalid
  if (!isMaster(seq[0])) {
    masterState_ = SequenceState::err_source_address;
    return;
  }

  // target address is invalid
  if (!isTarget(seq[1])) {
    masterState_ = SequenceState::err_target_address;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[4]) > max_bytes) {
    masterState_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(5 + uint8_t(seq[4]))) {
    masterState_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(5 + uint8_t(seq[4]) + 1)) {
    masterState_ = SequenceState::err_seq_too_long;
    return;
  }

  telegramType_ = typeOf(seq[1]);
  masterNN_ = static_cast<size_t>(uint8_t(seq[4]));

  if (seq.size() == static_cast<size_t>(5 + masterNN_)) {
    master_ = seq;
    masterCRC_ = seq.crc();
  } else {
    master_ = Sequence(seq, 0, 5 + masterNN_);
    masterCRC_ = seq[5 + masterNN_];

    // CRC byte is invalid
    if (master_.crc() != masterCRC_)
      masterState_ = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::createSlave(const std::vector<uint8_t>& vec) {
  Sequence seq;

  for (size_t i = 0; i < vec.size(); i++) seq.push_back(vec.at(i), false);

  createSlave(seq);
}

void ebus::Telegram::createSlave(Sequence& seq) {
  slaveState_ = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < static_cast<size_t>(2)) {
    slaveState_ = SequenceState::err_seq_too_short;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[0]) > max_bytes) {
    slaveState_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(1 + uint8_t(seq[0]))) {
    slaveState_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(1 + uint8_t(seq[0]) + 1)) {
    slaveState_ = SequenceState::err_seq_too_long;
    return;
  }

  slaveNN_ = static_cast<size_t>(uint8_t(seq[0]));

  if (seq.size() == (1 + slaveNN_)) {
    slave_ = seq;
    slaveCRC_ = seq.crc();
  } else {
    slave_ = Sequence(seq, 0, 1 + slaveNN_);
    slaveCRC_ = seq[1 + slaveNN_];

    // CRC byte is invalid
    if (slave_.crc() != slaveCRC_) slaveState_ = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::clear() {
  telegramType_ = TelegramType::undefined;

  master_.clear();
  masterNN_ = 0;
  masterCRC_ = sym_zero;
  masterACK_ = sym_zero;
  masterState_ = SequenceState::seq_empty;

  slave_.clear();
  slaveNN_ = 0;
  slaveCRC_ = sym_zero;
  slaveACK_ = sym_zero;
  slaveState_ = SequenceState::seq_empty;
}

const ebus::Sequence& ebus::Telegram::getMaster() const { return master_; }

uint8_t ebus::Telegram::getSourceAddress() const { return master_[0]; }

uint8_t ebus::Telegram::getTargetAddress() const { return master_[1]; }

uint8_t ebus::Telegram::getPrimaryCommand() const { return master_[2]; }

uint8_t ebus::Telegram::getSecondaryCommand() const { return master_[3]; }

uint8_t ebus::Telegram::getMasterNumberBytes() const { return master_[4]; }

const std::vector<uint8_t> ebus::Telegram::getMasterDataBytes() const {
  return master_.range(5, master_.size() - 5);
}

uint8_t ebus::Telegram::getMasterCRC() const { return masterCRC_; }

ebus::SequenceState ebus::Telegram::getMasterState() const {
  return masterState_;
}

void ebus::Telegram::setMasterACK(const uint8_t byte) { masterACK_ = byte; }

uint8_t ebus::Telegram::getMasterACK() const { return masterACK_; }

const ebus::Sequence& ebus::Telegram::getSlave() const { return slave_; }

uint8_t ebus::Telegram::getSlaveNumberBytes() const { return slave_[0]; }

const std::vector<uint8_t> ebus::Telegram::getSlaveDataBytes() const {
  return slave_.range(1, slave_.size() - 1);
}

uint8_t ebus::Telegram::getSlaveCRC() const { return slaveCRC_; }

ebus::SequenceState ebus::Telegram::getSlaveState() const {
  return slaveState_;
}

void ebus::Telegram::setSlaveACK(const uint8_t byte) { slaveACK_ = byte; }

uint8_t ebus::Telegram::getSlaveACK() const { return slaveACK_; }

ebus::TelegramType ebus::Telegram::getType() const { return telegramType_; }

bool ebus::Telegram::isValid() const {
  if (telegramType_ != TelegramType::master_slave)
    return masterState_ == SequenceState::seq_ok;

  return (masterState_ == SequenceState::seq_ok &&
          slaveState_ == SequenceState::seq_ok);
}

const std::string ebus::Telegram::to_string() const {
  std::ostringstream ostr;

  ostr << toStringMaster();

  if (masterState_ == SequenceState::seq_ok &&
      telegramType_ == TelegramType::master_slave)
    ostr << " " << toStringSlave();

  return ostr.str();
}

const std::string ebus::Telegram::toStringMaster() const {
  std::ostringstream ostr;
  if (masterState_ != SequenceState::seq_ok)
    ostr << toStringMasterState();
  else
    ostr << master_.to_string();

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlave() const {
  std::ostringstream ostr;
  if (slaveState_ != SequenceState::seq_ok &&
      telegramType_ != TelegramType::broadcast) {
    ostr << toStringSlaveState();
  } else {
    ostr << slave_.to_string();
  }

  return ostr.str();
}

const std::string ebus::Telegram::toStringMasterState() const {
  std::ostringstream ostr;
  if (master_.size() > 0) ostr << "'" << master_.to_string() << "' ";

  ostr << "master " << ebus::getSequenceStateText(masterState_);

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlaveState() const {
  std::ostringstream ostr;
  if (slave_.size() > 0) ostr << "'" << slave_.to_string() << "' ";

  ostr << "slave " << getSequenceStateText(slaveState_);

  return ostr.str();
}

ebus::SequenceState ebus::Telegram::checkMasterSequence(const Sequence& seq) {
  // sequence is too short
  if (seq.size() < static_cast<size_t>(5))
    return SequenceState::err_seq_too_short;

  // source address is invalid
  if (!isMaster(seq[0])) return SequenceState::err_source_address;

  // target address is invalid
  if (!isTarget(seq[1])) return SequenceState::err_target_address;

  // data byte is invalid
  if (uint8_t(seq[4]) > max_bytes) return SequenceState::err_data_byte;

  // sequence is too short (incl. CRC)
  if (seq.size() < static_cast<size_t>(5 + uint8_t(seq[4]) + 1))
    return SequenceState::err_seq_too_short;

  return SequenceState::seq_ok;
}

ebus::SequenceState ebus::Telegram::checkSlaveSequence(const Sequence& seq) {
  // sequence is too short
  if (seq.size() < static_cast<size_t>(1))
    return SequenceState::err_seq_too_short;

  // data byte is invalid
  if (uint8_t(seq[0]) > max_bytes) return SequenceState::err_data_byte;

  // sequence is too short (incl. CRC)
  if (seq.size() < static_cast<size_t>(1 + uint8_t(seq[0]) + 1))
    return SequenceState::err_seq_too_short;

  return SequenceState::seq_ok;
}
