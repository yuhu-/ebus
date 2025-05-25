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

ebus::Telegram::Telegram(Sequence &seq) { parse(seq); }

void ebus::Telegram::parse(Sequence &seq) {
  clear();
  seq.reduce();
  int offset = 0;

  m_masterState = checkMasterSequence(seq);

  if (m_masterState != SequenceState::seq_ok) return;

  Sequence master(seq, 0, 5 + uint8_t(seq[4]) + 1);
  createMaster(master);

  if (m_masterState != SequenceState::seq_ok) return;

  if (m_type != TelegramType::broadcast) {
    // acknowledge byte is missing
    if (seq.size() <= static_cast<size_t>(5 + m_masterNN + 1)) {
      m_masterState = SequenceState::err_ack_missing;
      return;
    }

    m_masterACK = seq[5 + m_masterNN + 1];

    // acknowledge byte is invalid
    if (m_masterACK != sym_ack && m_masterACK != sym_nak) {
      m_masterState = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from slave
    if (m_masterACK == sym_nak) {
      // sequence is too short
      if (seq.size() < static_cast<size_t>(master.size() + 1)) {
        m_masterState = SequenceState::err_seq_too_short;
        return;
      }

      offset = master.size() + 1;
      m_master.clear();

      Sequence tmp(seq, offset);
      m_masterState = checkMasterSequence(tmp);

      if (m_masterState != SequenceState::seq_ok) return;

      Sequence master2(tmp, 0, 5 + uint8_t(tmp[4]) + 1);
      createMaster(master2);

      if (m_masterState != SequenceState::seq_ok) return;

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(5 + m_masterNN + 1)) {
        m_masterState = SequenceState::err_ack_missing;
        return;
      }

      m_masterACK = tmp[5 + m_masterNN + 1];

      // acknowledge byte is invalid
      if (m_masterACK != sym_ack && m_masterACK != sym_nak) {
        m_masterState = SequenceState::err_ack_invalid;
        return;
      }

      // handle second NAK from slave
      if (m_masterACK == sym_nak) {
        // // sequence is too long
        // if (tmp.size() > static_cast<size_t>(5 + m_masterNN + 2))
        //   m_masterState = SequenceState::err_seq_too_long;
        // else

        // acknowledge byte is negative
        m_masterState = SequenceState::err_ack_negative;

        return;
      }
    }
  }

  if (m_type == TelegramType::master_slave) {
    offset += 5 + m_masterNN + 2;

    Sequence seq2(seq, offset);
    m_slaveState = checkSlaveSequence(seq2);

    if (m_slaveState != SequenceState::seq_ok) return;

    Sequence slave(seq2, 0, 1 + uint8_t(seq2[0]) + 1);
    createSlave(slave);

    if (m_slaveState != SequenceState::seq_ok) return;

    // acknowledge byte is missing
    if (seq2.size() <= static_cast<size_t>(1 + m_slaveNN + 1)) {
      m_slaveState = SequenceState::err_ack_missing;
      return;
    }

    m_slaveACK = seq2[1 + m_slaveNN + 1];

    // acknowledge byte is invalid
    if (m_slaveACK != sym_ack && m_slaveACK != sym_nak) {
      m_slaveState = SequenceState::err_ack_invalid;
      return;
    }

    // handle first NAK from master
    if (m_slaveACK == sym_nak) {
      // sequence is too short
      if (seq2.size() < static_cast<size_t>(slave.size() + 2)) {
        m_slaveState = SequenceState::err_seq_too_short;
        return;
      }

      offset = slave.size() + 1;
      m_slave.clear();

      Sequence tmp(seq2, offset);
      m_slaveState = checkSlaveSequence(tmp);

      if (m_slaveState != SequenceState::seq_ok) return;

      Sequence slave2(seq2, offset, 1 + uint8_t(seq2[offset]) + 1);
      createSlave(slave2);

      // acknowledge byte is missing
      if (tmp.size() <= static_cast<size_t>(1 + m_slaveNN + 1)) {
        m_slaveState = SequenceState::err_ack_missing;
        return;
      }

      m_slaveACK = tmp[1 + m_slaveNN + 1];

      // acknowledge byte is invalid
      if (m_slaveACK != sym_ack && m_slaveACK != sym_nak) {
        m_slaveState = SequenceState::err_ack_invalid;
        return;
      }

      // sequence is too long
      // if (tmp.size() > static_cast<size_t>(1 + m_slaveNN + 2)) {
      //   m_slaveState = SequenceState::err_seq_too_long;
      //   m_slave.clear();
      //   return;
      // }

      // handle second NAK from master
      if (m_slaveACK == sym_nak) {
        // acknowledge byte is negative
        m_slaveState = SequenceState::err_ack_negative;
        return;
      }
    }
  }
}

void ebus::Telegram::createMaster(const uint8_t src,
                                  const std::vector<uint8_t> &vec) {
  Sequence seq;

  seq.push_back(src, false);

  for (size_t i = 0; i < vec.size(); i++) seq.push_back(vec.at(i), false);

  createMaster(seq);
}

void ebus::Telegram::createMaster(Sequence &seq) {
  m_masterState = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < 5) {
    m_masterState = SequenceState::err_seq_too_short;
    return;
  }

  // source address is invalid
  if (!isMaster(seq[0])) {
    m_masterState = SequenceState::err_source_address;
    return;
  }

  // target address is invalid
  if (!isTarget(seq[1])) {
    m_masterState = SequenceState::err_target_address;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[4]) > max_bytes) {
    m_masterState = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(5 + uint8_t(seq[4]))) {
    m_masterState = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(5 + uint8_t(seq[4]) + 1)) {
    m_masterState = SequenceState::err_seq_too_long;
    return;
  }

  m_type = typeOf(seq[1]);
  m_masterNN = static_cast<size_t>(uint8_t(seq[4]));

  if (seq.size() == static_cast<size_t>(5 + m_masterNN)) {
    m_master = seq;
    m_masterCRC = seq.crc();
  } else {
    m_master = Sequence(seq, 0, 5 + m_masterNN);
    m_masterCRC = seq[5 + m_masterNN];

    // CRC byte is invalid
    if (m_master.crc() != m_masterCRC)
      m_masterState = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::createSlave(const std::vector<uint8_t> &vec) {
  Sequence seq;

  for (size_t i = 0; i < vec.size(); i++) seq.push_back(vec.at(i), false);

  createSlave(seq);
}

void ebus::Telegram::createSlave(Sequence &seq) {
  m_slaveState = SequenceState::seq_ok;
  seq.reduce();

  // sequence is too short
  if (seq.size() < static_cast<size_t>(2)) {
    m_slaveState = SequenceState::err_seq_too_short;
    return;
  }

  // data byte is invalid
  if (uint8_t(seq[0]) > max_bytes) {
    m_slaveState = SequenceState::err_data_byte;
    return;
  }

  // sequence is too short (excl. CRC)
  if (seq.size() < static_cast<size_t>(1 + uint8_t(seq[0]))) {
    m_slaveState = SequenceState::err_seq_too_short;
    return;
  }

  // sequence is too long (incl. CRC)
  if (seq.size() > static_cast<size_t>(1 + uint8_t(seq[0]) + 1)) {
    m_slaveState = SequenceState::err_seq_too_long;
    return;
  }

  m_slaveNN = static_cast<size_t>(uint8_t(seq[0]));

  if (seq.size() == (1 + m_slaveNN)) {
    m_slave = seq;
    m_slaveCRC = seq.crc();
  } else {
    m_slave = Sequence(seq, 0, 1 + m_slaveNN);
    m_slaveCRC = seq[1 + m_slaveNN];

    // CRC byte is invalid
    if (m_slave.crc() != m_slaveCRC)
      m_slaveState = SequenceState::err_crc_invalid;
  }
}

void ebus::Telegram::clear() {
  m_type = TelegramType::undefined;

  m_master.clear();
  m_masterNN = 0;
  m_masterCRC = sym_zero;
  m_masterACK = sym_zero;
  m_masterState = SequenceState::seq_empty;

  m_slave.clear();
  m_slaveNN = 0;
  m_slaveCRC = sym_zero;
  m_slaveACK = sym_zero;
  m_slaveState = SequenceState::seq_empty;
}

const ebus::Sequence &ebus::Telegram::getMaster() const { return m_master; }

uint8_t ebus::Telegram::getSourceAddress() const { return m_master[0]; }

uint8_t ebus::Telegram::getTargetAddress() const { return m_master[1]; }

uint8_t ebus::Telegram::getPrimaryCommand() const { return m_master[2]; }

uint8_t ebus::Telegram::getSecondaryCommand() const { return m_master[3]; }

uint8_t ebus::Telegram::getMasterNumberBytes() const { return m_master[4]; }

const std::vector<uint8_t> ebus::Telegram::getMasterDataBytes() const {
  return m_master.range(5, m_master.size() - 5);
}

uint8_t ebus::Telegram::getMasterCRC() const { return m_masterCRC; }

ebus::SequenceState ebus::Telegram::getMasterState() const {
  return m_masterState;
}

void ebus::Telegram::setMasterACK(const uint8_t byte) { m_masterACK = byte; }

uint8_t ebus::Telegram::getMasterACK() const { return m_masterACK; }

const ebus::Sequence &ebus::Telegram::getSlave() const { return m_slave; }

uint8_t ebus::Telegram::getSlaveNumberBytes() const { return m_slave[0]; }

const std::vector<uint8_t> ebus::Telegram::getSlaveDataBytes() const {
  return m_slave.range(1, m_slave.size() - 1);
}

uint8_t ebus::Telegram::getSlaveCRC() const { return m_slaveCRC; }

ebus::SequenceState ebus::Telegram::getSlaveState() const {
  return m_slaveState;
}

void ebus::Telegram::setSlaveACK(const uint8_t byte) { m_slaveACK = byte; }

uint8_t ebus::Telegram::getSlaveACK() const { return m_slaveACK; }

ebus::TelegramType ebus::Telegram::getType() const { return m_type; }

bool ebus::Telegram::isValid() const {
  if (m_type != TelegramType::master_slave)
    return m_masterState == SequenceState::seq_ok;

  return (m_masterState == SequenceState::seq_ok &&
          m_slaveState == SequenceState::seq_ok);
}

const std::string ebus::Telegram::to_string() const {
  std::ostringstream ostr;

  ostr << toStringMaster();

  if (m_masterState == SequenceState::seq_ok &&
      m_type == TelegramType::master_slave)
    ostr << " " << toStringSlave();

  return ostr.str();
}

const std::string ebus::Telegram::toStringMaster() const {
  std::ostringstream ostr;
  if (m_masterState != SequenceState::seq_ok)
    ostr << toStringMasterState();
  else
    ostr << m_master.to_string();

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlave() const {
  std::ostringstream ostr;
  if (m_slaveState != SequenceState::seq_ok &&
      m_type != TelegramType::broadcast) {
    ostr << toStringSlaveState();
  } else {
    ostr << m_slave.to_string();
  }

  return ostr.str();
}

const std::string ebus::Telegram::toStringMasterState() const {
  std::ostringstream ostr;
  if (m_master.size() > 0) ostr << "'" << m_master.to_string() << "' ";

  ostr << "master " << ebus::getSequenceStateText(m_masterState);

  return ostr.str();
}

const std::string ebus::Telegram::toStringSlaveState() const {
  std::ostringstream ostr;
  if (m_slave.size() > 0) ostr << "'" << m_slave.to_string() << "' ";

  ostr << "slave " << getSequenceStateText(m_slaveState);

  return ostr.str();
}

ebus::SequenceState ebus::Telegram::checkMasterSequence(const Sequence &seq) {
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

ebus::SequenceState ebus::Telegram::checkSlaveSequence(const Sequence &seq) {
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
