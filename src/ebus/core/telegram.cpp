/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/telegram.hpp"

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

  master_state_ = checkMasterSequence(seq);

  if (master_state_ != SequenceState::seq_ok) return;

  Sequence master_seq(seq, 0, 5 + uint8_t(seq[4]) + 1);
  createMaster(master_seq);

  if (master_state_ != SequenceState::seq_ok) return;

  if (telegram_type_ != TelegramType::broadcast) {
    // acknowledge byte is missing
    if (seq.size() <= static_cast<size_t>(5 + master_nn_ + 1)) {
      master_state_ = SequenceState::err_ack_missing;
      return;
    }

    master_ack_ = seq[5 + master_nn_ + 1];

    // acknowledge byte is invalid
    if (master_ack_ != sym_ack && master_ack_ != sym_nak) {
      master_state_ = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from slave
    if (master_ack_ == sym_nak) {
      // sequence is too short
      if (seq.size() < static_cast<size_t>(master_.size() + 1)) {
        master_state_ = SequenceState::err_seq_too_short;
        return;
      }

      // The master sequence (master_) contains the payload [QQ ZZ PB SB NN
      // Data]. It does NOT include the CRC. The raw sequence 'seq' structure
      // is: [ ... master_ ... ] [CRC] [ACK] [Next Attempt...] To jump to the
      // next attempt, we skip the payload (master_.size()), plus 1 byte for CRC
      // and 1 byte for ACK.
      offset = master_.size() + 2;
      master_.clear();

      Sequence tmp(seq, offset);
      master_state_ = checkMasterSequence(tmp);

      if (master_state_ != SequenceState::seq_ok) return;

      Sequence master_seq_2(tmp, 0, 5 + uint8_t(tmp[4]) + 1);
      createMaster(master_seq_2);

      if (master_state_ != SequenceState::seq_ok) return;

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(5 + master_nn_ + 1)) {
        master_state_ = SequenceState::err_ack_missing;
        return;
      }

      master_ack_ = tmp[5 + master_nn_ + 1];

      // acknowledge byte is invalid
      if (master_ack_ != sym_ack && master_ack_ != sym_nak) {
        master_state_ = SequenceState::err_ack_invalid;
        return;
      }

      // handle second NAK from slave
      if (master_ack_ == sym_nak) {
        // // sequence is too long
        // if (tmp.size() > static_cast<size_t>(5 + master_nn_ + 2))
        //   master_state_ = SequenceState::err_seq_too_long;
        // else

        // acknowledge byte is negative
        master_state_ = SequenceState::err_ack_negative;

        return;
      }
    }
  }

  if (telegram_type_ == TelegramType::master_slave) {
    // If this is a Master-Slave telegram, the Slave response follows the Master
    // ACK. Offset calculation: 5 (Header: QQ ZZ PB SB NN) + masterNN_ (Data) +
    // 1 (CRC) + 1 (ACK) = 5 + master_nn_ + 2
    offset += 5 + master_nn_ + 2;

    Sequence seq_2(seq, offset);
    slave_state_ = checkSlaveSequence(seq_2);

    if (slave_state_ != SequenceState::seq_ok) return;

    Sequence slave_seq(seq_2, 0, 1 + uint8_t(seq_2[0]) + 1);
    createSlave(slave_seq);

    if (slave_state_ != SequenceState::seq_ok) return;

    // acknowledge byte is missing
    if (seq_2.size() <= static_cast<size_t>(1 + slave_nn_ + 1)) {
      slave_state_ = SequenceState::err_ack_missing;
      return;
    }

    slave_ack_ = seq_2[1 + slave_nn_ + 1];

    // acknowledge byte is invalid
    if (slave_ack_ != sym_ack && slave_ack_ != sym_nak) {
      slave_state_ = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from master
    if (slave_ack_ == sym_nak) {
      // sequence is too short
      if (seq_2.size() < static_cast<size_t>(slave_.size() + 2)) {
        slave_state_ = SequenceState::err_seq_too_short;
        return;
      }

      // Same logic as for Master retry:
      // The slave sequence (slave_) contains [NN Data].
      // We skip the payload (slave_.size()), plus 1 byte for CRC and 1 byte for
      // ACK to find the start of the retry sequence.
      offset = slave_.size() + 2;
      slave_.clear();

      Sequence tmp(seq_2, offset);
      slave_state_ = checkSlaveSequence(tmp);

      if (slave_state_ != SequenceState::seq_ok) return;

      Sequence slave_seq_2(seq_2, offset, 1 + uint8_t(seq_2[offset]) + 1);
      createSlave(slave_seq_2);

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(1 + slave_nn_ + 1)) {
        slave_state_ = SequenceState::err_ack_missing;
        return;
      }

      slave_ack_ = tmp[1 + slave_nn_ + 1];

      // acknowledge byte is invalid
      if (slave_ack_ != sym_ack && slave_ack_ != sym_nak) {
        slave_state_ = SequenceState::err_ack_invalid;
        return;
      }

      // sequence is too long
      // if (tmp.size() > static_cast<size_t>(1 + slaveNN_ + 2)) {
      //   slaveState_ = SequenceState::err_seq_too_long;
      //   slave.clear();
      //   return;
      // }

      // handle second NAK from master
      if (slave_ack_ == sym_nak) {
        // acknowledge byte is negative
        slave_state_ = SequenceState::err_ack_negative;
        return;
      }
    }
  }
}

void ebus::Telegram::createMaster(const uint8_t src,
                                  const std::vector<uint8_t>& vec) {
  Sequence seq;

  seq.pushBack(src, false);

  for (size_t i = 0; i < vec.size(); i++) seq.pushBack(vec.at(i), false);

  createMaster(seq);
}

void ebus::Telegram::createMaster(Sequence& seq) {
  master_state_ = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < 5) {
    master_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // source address is invalid
  if (!isMaster(seq[0])) {
    master_state_ = SequenceState::err_source_address;
    return;
  }

  // target address is invalid
  if (!isTarget(seq[1])) {
    master_state_ = SequenceState::err_target_address;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[4]) > max_bytes) {
    master_state_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(5 + uint8_t(seq[4]))) {
    master_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(5 + uint8_t(seq[4]) + 1)) {
    master_state_ = SequenceState::err_seq_too_long;
    return;
  }

  telegram_type_ = typeOf(seq[1]);
  master_nn_ = static_cast<size_t>(uint8_t(seq[4]));

  if (seq.size() == static_cast<size_t>(5 + master_nn_)) {
    master_ = seq;
    master_crc_ = seq.crc();
  } else {
    master_ = Sequence(seq, 0, 5 + master_nn_);
    master_crc_ = seq[5 + master_nn_];

    // CRC byte is invalid
    if (master_.crc() != master_crc_)
      master_state_ = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::createSlave(const std::vector<uint8_t>& vec) {
  Sequence seq;

  for (size_t i = 0; i < vec.size(); i++) seq.pushBack(vec.at(i), false);

  createSlave(seq);
}

void ebus::Telegram::createSlave(Sequence& seq) {
  slave_state_ = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < static_cast<size_t>(2)) {
    slave_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[0]) > max_bytes) {
    slave_state_ = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(1 + uint8_t(seq[0]))) {
    slave_state_ = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(1 + uint8_t(seq[0]) + 1)) {
    slave_state_ = SequenceState::err_seq_too_long;
    return;
  }

  slave_nn_ = static_cast<size_t>(uint8_t(seq[0]));

  if (seq.size() == (1 + slave_nn_)) {
    slave_ = seq;
    slave_crc_ = seq.crc();
  } else {
    slave_ = Sequence(seq, 0, 1 + slave_nn_);
    slave_crc_ = seq[1 + slave_nn_];

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

const std::vector<uint8_t> ebus::Telegram::getMasterDataBytes() const {
  return master_.range(5, master_.size() - 5);
}

uint8_t ebus::Telegram::getMasterCRC() const { return master_crc_; }

ebus::SequenceState ebus::Telegram::getMasterState() const {
  return master_state_;
}

void ebus::Telegram::setMasterACK(const uint8_t byte) { master_ack_ = byte; }

uint8_t ebus::Telegram::getMasterACK() const { return master_ack_; }

const ebus::Sequence& ebus::Telegram::getSlave() const { return slave_; }

uint8_t ebus::Telegram::getSlaveNumberBytes() const { return slave_[0]; }

const std::vector<uint8_t> ebus::Telegram::getSlaveDataBytes() const {
  return slave_.range(1, slave_.size() - 1);
}

uint8_t ebus::Telegram::getSlaveCRC() const { return slave_crc_; }

ebus::SequenceState ebus::Telegram::getSlaveState() const {
  return slave_state_;
}

void ebus::Telegram::setSlaveACK(const uint8_t byte) { slave_ack_ = byte; }

uint8_t ebus::Telegram::getSlaveACK() const { return slave_ack_; }

ebus::TelegramType ebus::Telegram::getType() const { return telegram_type_; }

bool ebus::Telegram::isValid() const {
  if (telegram_type_ != TelegramType::master_slave)
    return master_state_ == SequenceState::seq_ok;

  return (master_state_ == SequenceState::seq_ok &&
          slave_state_ == SequenceState::seq_ok);
}

const std::string ebus::Telegram::toString() const {
  std::ostringstream ostr;

  ostr << toStringMaster();

  if (master_state_ == SequenceState::seq_ok &&
      telegram_type_ == TelegramType::master_slave)
    ostr << " " << toStringSlave();

  return ostr.str();
}

const std::string ebus::Telegram::toStringMaster() const {
  std::ostringstream ostr;
  if (master_state_ != SequenceState::seq_ok)
    ostr << toStringMasterState();
  else
    ostr << master_.toString();

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlave() const {
  std::ostringstream ostr;
  if (slave_state_ != SequenceState::seq_ok &&
      telegram_type_ != TelegramType::broadcast) {
    ostr << toStringSlaveState();
  } else {
    ostr << slave_.toString();
  }

  return ostr.str();
}

const std::string ebus::Telegram::toStringMasterState() const {
  std::ostringstream ostr;
  if (master_.size() > 0) ostr << "'" << master_.toString() << "' ";

  ostr << "master " << ebus::getSequenceStateText(master_state_);

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlaveState() const {
  std::ostringstream ostr;
  if (slave_.size() > 0) ostr << "'" << slave_.toString() << "' ";

  ostr << "slave " << getSequenceStateText(slave_state_);

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