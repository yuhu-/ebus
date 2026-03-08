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

  masterState = checkMasterSequence(seq);

  if (masterState != SequenceState::seq_ok) return;

  Sequence masterSeq(seq, 0, 5 + uint8_t(seq[4]) + 1);
  createMaster(masterSeq);

  if (masterState != SequenceState::seq_ok) return;

  if (type != TelegramType::broadcast) {
    // acknowledge byte is missing
    if (seq.size() <= static_cast<size_t>(5 + masterNN + 1)) {
      masterState = SequenceState::err_ack_missing;
      return;
    }

    masterACK = seq[5 + masterNN + 1];

    // acknowledge byte is invalid
    if (masterACK != sym_ack && masterACK != sym_nak) {
      masterState = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from slave
    if (masterACK == sym_nak) {
      // sequence is too short
      if (seq.size() < static_cast<size_t>(master.size() + 1)) {
        masterState = SequenceState::err_seq_too_short;
        return;
      }

      offset = master.size() + 1;
      master.clear();

      Sequence tmp(seq, offset);
      masterState = checkMasterSequence(tmp);

      if (masterState != SequenceState::seq_ok) return;

      Sequence masterSeq2(tmp, 0, 5 + uint8_t(tmp[4]) + 1);
      createMaster(masterSeq2);

      if (masterState != SequenceState::seq_ok) return;

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(5 + masterNN + 1)) {
        masterState = SequenceState::err_ack_missing;
        return;
      }

      masterACK = tmp[5 + masterNN + 1];

      // acknowledge byte is invalid
      if (masterACK != sym_ack && masterACK != sym_nak) {
        masterState = SequenceState::err_ack_invalid;
        return;
      }

      // handle second NAK from slave
      if (masterACK == sym_nak) {
        // // sequence is too long
        // if (tmp.size() > static_cast<size_t>(5 + masterNN + 2))
        //   masterState = SequenceState::err_seq_too_long;
        // else

        // acknowledge byte is negative
        masterState = SequenceState::err_ack_negative;

        return;
      }
    }
  }

  if (type == TelegramType::master_slave) {
    offset += 5 + masterNN + 2;

    Sequence seq2(seq, offset);
    slaveState = checkSlaveSequence(seq2);

    if (slaveState != SequenceState::seq_ok) return;

    Sequence slaveSeq(seq2, 0, 1 + uint8_t(seq2[0]) + 1);
    createSlave(slaveSeq);

    if (slaveState != SequenceState::seq_ok) return;

    // acknowledge byte is missing
    if (seq2.size() <= static_cast<size_t>(1 + slaveNN + 1)) {
      slaveState = SequenceState::err_ack_missing;
      return;
    }

    slaveACK = seq2[1 + slaveNN + 1];

    // acknowledge byte is invalid
    if (slaveACK != sym_ack && slaveACK != sym_nak) {
      slaveState = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from master
    if (slaveACK == sym_nak) {
      // sequence is too short
      if (seq2.size() < static_cast<size_t>(slave.size() + 2)) {
        slaveState = SequenceState::err_seq_too_short;
        return;
      }

      offset = slave.size() + 1;
      slave.clear();

      Sequence tmp(seq2, offset);
      slaveState = checkSlaveSequence(tmp);

      if (slaveState != SequenceState::seq_ok) return;

      Sequence slaveSeq2(seq2, offset, 1 + uint8_t(seq2[offset]) + 1);
      createSlave(slaveSeq2);

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(1 + slaveNN + 1)) {
        slaveState = SequenceState::err_ack_missing;
        return;
      }

      slaveACK = tmp[1 + slaveNN + 1];

      // acknowledge byte is invalid
      if (slaveACK != sym_ack && slaveACK != sym_nak) {
        slaveState = SequenceState::err_ack_invalid;
        return;
      }

      // sequence is too long
      // if (tmp.size() > static_cast<size_t>(1 + slaveNN + 2)) {
      //   slaveState = SequenceState::err_seq_too_long;
      //   slave.clear();
      //   return;
      // }

      // handle second NAK from master
      if (slaveACK == sym_nak) {
        // acknowledge byte is negative
        slaveState = SequenceState::err_ack_negative;
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
  masterState = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < 5) {
    masterState = SequenceState::err_seq_too_short;
    return;
  }

  // source address is invalid
  if (!isMaster(seq[0])) {
    masterState = SequenceState::err_source_address;
    return;
  }

  // target address is invalid
  if (!isTarget(seq[1])) {
    masterState = SequenceState::err_target_address;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[4]) > max_bytes) {
    masterState = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(5 + uint8_t(seq[4]))) {
    masterState = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(5 + uint8_t(seq[4]) + 1)) {
    masterState = SequenceState::err_seq_too_long;
    return;
  }

  type = typeOf(seq[1]);
  masterNN = static_cast<size_t>(uint8_t(seq[4]));

  if (seq.size() == static_cast<size_t>(5 + masterNN)) {
    master = seq;
    masterCRC = seq.crc();
  } else {
    master = Sequence(seq, 0, 5 + masterNN);
    masterCRC = seq[5 + masterNN];

    // CRC byte is invalid
    if (master.crc() != masterCRC) masterState = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::createSlave(const std::vector<uint8_t>& vec) {
  Sequence seq;

  for (size_t i = 0; i < vec.size(); i++) seq.push_back(vec.at(i), false);

  createSlave(seq);
}

void ebus::Telegram::createSlave(Sequence& seq) {
  slaveState = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < static_cast<size_t>(2)) {
    slaveState = SequenceState::err_seq_too_short;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[0]) > max_bytes) {
    slaveState = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(1 + uint8_t(seq[0]))) {
    slaveState = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(1 + uint8_t(seq[0]) + 1)) {
    slaveState = SequenceState::err_seq_too_long;
    return;
  }

  slaveNN = static_cast<size_t>(uint8_t(seq[0]));

  if (seq.size() == (1 + slaveNN)) {
    slave = seq;
    slaveCRC = seq.crc();
  } else {
    slave = Sequence(seq, 0, 1 + slaveNN);
    slaveCRC = seq[1 + slaveNN];

    // CRC byte is invalid
    if (slave.crc() != slaveCRC) slaveState = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::clear() {
  type = TelegramType::undefined;

  master.clear();
  masterNN = 0;
  masterCRC = sym_zero;
  masterACK = sym_zero;
  masterState = SequenceState::seq_empty;

  slave.clear();
  slaveNN = 0;
  slaveCRC = sym_zero;
  slaveACK = sym_zero;
  slaveState = SequenceState::seq_empty;
}

const ebus::Sequence& ebus::Telegram::getMaster() const { return master; }

uint8_t ebus::Telegram::getSourceAddress() const { return master[0]; }

uint8_t ebus::Telegram::getTargetAddress() const { return master[1]; }

uint8_t ebus::Telegram::getPrimaryCommand() const { return master[2]; }

uint8_t ebus::Telegram::getSecondaryCommand() const { return master[3]; }

uint8_t ebus::Telegram::getMasterNumberBytes() const { return master[4]; }

const std::vector<uint8_t> ebus::Telegram::getMasterDataBytes() const {
  return master.range(5, master.size() - 5);
}

uint8_t ebus::Telegram::getMasterCRC() const { return masterCRC; }

ebus::SequenceState ebus::Telegram::getMasterState() const {
  return masterState;
}

void ebus::Telegram::setMasterACK(const uint8_t byte) { masterACK = byte; }

uint8_t ebus::Telegram::getMasterACK() const { return masterACK; }

const ebus::Sequence& ebus::Telegram::getSlave() const { return slave; }

uint8_t ebus::Telegram::getSlaveNumberBytes() const { return slave[0]; }

const std::vector<uint8_t> ebus::Telegram::getSlaveDataBytes() const {
  return slave.range(1, slave.size() - 1);
}

uint8_t ebus::Telegram::getSlaveCRC() const { return slaveCRC; }

ebus::SequenceState ebus::Telegram::getSlaveState() const { return slaveState; }

void ebus::Telegram::setSlaveACK(const uint8_t byte) { slaveACK = byte; }

uint8_t ebus::Telegram::getSlaveACK() const { return slaveACK; }

ebus::TelegramType ebus::Telegram::getType() const { return type; }

bool ebus::Telegram::isValid() const {
  if (type != TelegramType::master_slave)
    return masterState == SequenceState::seq_ok;

  return (masterState == SequenceState::seq_ok &&
          slaveState == SequenceState::seq_ok);
}

const std::string ebus::Telegram::to_string() const {
  std::ostringstream ostr;

  ostr << toStringMaster();

  if (masterState == SequenceState::seq_ok &&
      type == TelegramType::master_slave)
    ostr << " " << toStringSlave();

  return ostr.str();
}

const std::string ebus::Telegram::toStringMaster() const {
  std::ostringstream ostr;
  if (masterState != SequenceState::seq_ok)
    ostr << toStringMasterState();
  else
    ostr << master.to_string();

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlave() const {
  std::ostringstream ostr;
  if (slaveState != SequenceState::seq_ok && type != TelegramType::broadcast) {
    ostr << toStringSlaveState();
  } else {
    ostr << slave.to_string();
  }

  return ostr.str();
}

const std::string ebus::Telegram::toStringMasterState() const {
  std::ostringstream ostr;
  if (master.size() > 0) ostr << "'" << master.to_string() << "' ";

  ostr << "master " << ebus::getSequenceStateText(masterState);

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlaveState() const {
  std::ostringstream ostr;
  if (slave.size() > 0) ostr << "'" << slave.to_string() << "' ";

  ostr << "slave " << getSequenceStateText(slaveState);

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
