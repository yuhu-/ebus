/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#include "Telegram.h"

#include <iomanip>
#include <map>
#include <sstream>

std::map<int, std::string> SequenceErrors =
{

{ SEQ_EMPTY, "sequence is empty" },

{ SEQ_ERR_SHORT, "sequence is too short" },
{ SEQ_ERR_LONG, "sequence is too long" },
{ SEQ_ERR_NN, "number data byte is invalid" },
{ SEQ_ERR_CRC, "sequence has a CRC error" },
{ SEQ_ERR_ACK, "acknowledge byte is invalid" },
{ SEQ_ERR_QQ, "source address is invalid" },
{ SEQ_ERR_ZZ, "target address is invalid" },
{ SEQ_ERR_ACK_MISS, "acknowledge byte is missing" },
{ SEQ_ERR_INVALID, "sequence is invalid" } };

ebus::Telegram::Telegram(Sequence &seq)
{
	parseSequence(seq);
}

void ebus::Telegram::parseSequence(Sequence &seq)
{
	seq.reduce();
	int offset = 0;

	m_masterState = checkMasterSequence(seq);

	if (m_masterState != SEQ_OK) return;

	Sequence master(seq, 0, 5 + std::to_integer<int>(seq[4]) + 1);
	createMaster(master);

	if (m_masterState != SEQ_OK) return;

	if (m_type != TEL_TYPE_BC)
	{
		// acknowledge byte is missing
		if (seq.size() <= (size_t) (5 + m_masterNN + 1))
		{
			m_slaveState = SEQ_ERR_ACK_MISS;
			return;
		}

		m_slaveACK = seq[5 + m_masterNN + 1];

		// acknowledge byte is invalid
		if (m_slaveACK != seq_ack && m_slaveACK != seq_nak)
		{
			m_slaveState = SEQ_ERR_ACK;
			return;
		}

		// handle NAK from slave
		if (m_slaveACK == seq_nak)
		{
			// sequence is too short
			if (seq.size() < (size_t) (master.size() + 1))
			{
				m_masterState = SEQ_ERR_SHORT;
				return;
			}

			offset = master.size() + 1;
			m_master.clear();

			Sequence tmp(seq, offset);
			m_masterState = checkMasterSequence(tmp);

			if (m_masterState != SEQ_OK) return;

			Sequence master2(tmp, 0, 5 + std::to_integer<int>(tmp[4]) + 1);
			createMaster(master2);

			if (m_masterState != SEQ_OK) return;

			// acknowledge byte is missing
			if (tmp.size() <= (size_t) (5 + m_masterNN + 1))
			{
				m_slaveState = SEQ_ERR_ACK_MISS;
				return;
			}

			m_slaveACK = tmp[5 + m_masterNN + 1];

			// acknowledge byte is invalid
			if (m_slaveACK != seq_ack && m_slaveACK != seq_nak)
			{
				m_slaveState = SEQ_ERR_ACK;
				return;
			}

			// acknowledge byte is negativ
			if (m_slaveACK == seq_nak)
			{
				// sequence is too long
				if (tmp.size() > (size_t) (5 + m_masterNN + 2))
					m_masterState = SEQ_ERR_LONG;

				// sequence is invalid
				else
					m_masterState = SEQ_ERR_INVALID;

				return;
			}
		}
	}

	if (m_type == TEL_TYPE_MS)
	{
		offset += 5 + m_masterNN + 2;

		Sequence seq2(seq, offset);
		m_slaveState = checkSlaveSequence(seq2);

		if (m_slaveState != SEQ_OK) return;

		Sequence slave(seq2, 0, 1 + std::to_integer<int>(seq2[0]) + 1);
		createSlave(slave);

		if (m_slaveState != SEQ_OK) return;

		// acknowledge byte is missing
		if (seq2.size() <= (size_t) (1 + m_slaveNN + 1))
		{
			m_masterState = SEQ_ERR_ACK_MISS;
			return;
		}

		m_masterACK = seq2[1 + m_slaveNN + 1];

		// acknowledge byte is invalid
		if (m_masterACK != seq_ack && m_masterACK != seq_nak)
		{
			m_masterState = SEQ_ERR_ACK;
			return;
		}

		// handle NAK from master
		if (m_masterACK == seq_nak)
		{
			// sequence is too short
			if (seq2.size() < (size_t) (slave.size() + 2))
			{
				m_slaveState = SEQ_ERR_SHORT;
				return;
			}

			offset = slave.size() + 2;
			m_slave.clear();

			Sequence tmp(seq2, offset);
			m_slaveState = checkSlaveSequence(tmp);

			if (m_slaveState != SEQ_OK) return;

			Sequence slave2(seq2, offset, 1 + std::to_integer<int>(seq2[offset]) + 1);
			createSlave(slave2);

			// acknowledge byte is missing
			if (tmp.size() <= (size_t) (1 + m_slaveNN + 1))
			{
				m_masterState = SEQ_ERR_ACK_MISS;
				return;
			}

			m_masterACK = tmp[1 + m_slaveNN + 1];

			// acknowledge byte is invalid
			if (m_masterACK != seq_ack && m_masterACK != seq_nak)
			{
				m_masterState = SEQ_ERR_ACK;
				return;
			}

			// sequence is too long
			if (tmp.size() > (size_t) (1 + m_slaveNN + 2))
			{
				m_slaveState = SEQ_ERR_LONG;
				m_slave.clear();
				return;
			}

			// acknowledge byte is negativ
			if (m_masterACK == seq_nak)
			{
				// sequence is invalid
				m_slaveState = SEQ_ERR_INVALID;
				return;
			}

		}
	}
}

void ebus::Telegram::createMaster(const std::byte source, const std::byte target, const std::string &str)
{
	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(source);
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(target);
	ostr << std::nouppercase << std::setw(0) << str;
	createMaster(ostr.str());
}

void ebus::Telegram::createMaster(const std::byte source, const std::string &str)
{
	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(source);
	ostr << std::nouppercase << std::setw(0) << str;
	createMaster(ostr.str());
}

void ebus::Telegram::createMaster(const std::string &str)
{
	std::ostringstream result;

	if (Sequence::isHex(str, result, 2))
	{
		Sequence seq(str);
		createMaster(seq);
	}
	else
	{
		m_masterState = SEQ_ERR_INVALID;
	}
}

void ebus::Telegram::createMaster(const std::byte source, const std::vector<std::byte> &vec)
{
	Sequence seq;

	seq.push_back(source);

	for (size_t i = 0; i < seq.size(); i++)
		seq.push_back(vec.at(i));

	createMaster(seq);
}

void ebus::Telegram::createMaster(Sequence &seq)
{
	m_masterState = SEQ_OK;
	seq.reduce();

	// sequence is too short
	if (seq.size() < 6)
	{
		m_masterState = SEQ_ERR_SHORT;
		return;
	}

	// source address is invalid
	if (isMaster(seq[0]) == false)
	{
		m_masterState = SEQ_ERR_QQ;
		return;
	}

	// target address is invalid
	if (isAddressValid(seq[1]) == false)
	{
		m_masterState = SEQ_ERR_ZZ;
		return;
	}

	// number data byte is invalid
	if (std::to_integer<int>(seq[4]) > seq_max_bytes)
	{
		m_masterState = SEQ_ERR_NN;
		return;
	}

	// sequence is too short (excl. CRC)
	if (seq.size() < (size_t) (5 + std::to_integer<int>(seq[4])))
	{
		m_masterState = SEQ_ERR_SHORT;
		return;
	}

	// sequence is too long (incl. CRC)
	if (seq.size() > (size_t) (5 + std::to_integer<int>(seq[4]) + 1))
	{
		m_masterState = SEQ_ERR_LONG;
		return;
	}

	m_masterQQ = seq[0];
	m_masterZZ = seq[1];
	setType(seq[1]);
	m_masterNN = (size_t) std::to_integer<int>(seq[4]);

	if (seq.size() == (size_t) (5 + m_masterNN))
	{
		m_master = seq;
		m_masterCRC = seq.getCRC();
	}
	else
	{
		m_master = Sequence(seq, 0, 5 + m_masterNN);
		m_masterCRC = seq[5 + m_masterNN];

		// sequence has a CRC error
		if (m_master.getCRC() != m_masterCRC) m_masterState = SEQ_ERR_CRC;
	}
}

void ebus::Telegram::createSlave(const std::string &str)
{
	std::ostringstream result;

	if (Sequence::isHex(str, result, 2))
	{
		Sequence seq(str);
		createSlave(seq);
	}
	else
	{
		m_slaveState = SEQ_ERR_INVALID;
	}
}

void ebus::Telegram::createSlave(Sequence &seq)
{
	m_slaveState = SEQ_OK;
	seq.reduce();

	// sequence is too short
	if (seq.size() < (size_t) 2)
	{
		m_slaveState = SEQ_ERR_SHORT;
		return;
	}

	// number data byte is invalid
	if (std::to_integer<int>(seq[0]) > seq_max_bytes)
	{
		m_slaveState = SEQ_ERR_NN;
		return;
	}

	// sequence is too short (excl. CRC)
	if (seq.size() < (size_t) (1 + std::to_integer<int>(seq[0])))
	{
		m_slaveState = SEQ_ERR_SHORT;
		return;
	}

	// sequence is too long (incl. CRC)
	if (seq.size() > (size_t) (1 + std::to_integer<int>(seq[0]) + 1))
	{
		m_slaveState = SEQ_ERR_LONG;
		return;
	}

	m_slaveNN = (size_t) std::to_integer<int>(seq[0]);

	if (seq.size() == (1 + m_slaveNN))
	{
		m_slave = seq;
		m_slaveCRC = seq.getCRC();
	}
	else
	{
		m_slave = Sequence(seq, 0, 1 + m_slaveNN);
		m_slaveCRC = seq[1 + m_slaveNN];

		// sequence has a CRC error
		if (m_slave.getCRC() != m_slaveCRC) m_slaveState = SEQ_ERR_CRC;
	}
}

void ebus::Telegram::clear()
{
	m_type = -1;

	m_masterQQ = seq_zero;
	m_masterZZ = seq_zero;

	m_master.clear();
	m_masterNN = 0;
	m_masterCRC = seq_zero;
	m_masterACK = seq_zero;
	m_masterState = SEQ_EMPTY;

	m_slave.clear();
	m_slaveNN = 0;
	m_slaveCRC = seq_zero;
	m_slaveACK = seq_zero;
	m_slaveState = SEQ_EMPTY;
}

std::byte ebus::Telegram::getMasterQQ() const
{
	return (m_masterQQ);
}

std::byte ebus::Telegram::getMasterZZ() const
{
	return (m_masterZZ);
}

ebus::Sequence ebus::Telegram::getMaster() const
{
	return (m_master);
}

size_t ebus::Telegram::getMasterNN() const
{
	return (m_masterNN);
}

std::byte ebus::Telegram::getMasterCRC() const
{
	return (m_masterCRC);
}

int ebus::Telegram::getMasterState() const
{
	return (m_masterState);
}

void ebus::Telegram::setSlaveACK(const std::byte byte)
{
	m_slaveACK = byte;
}

ebus::Sequence ebus::Telegram::getSlave() const
{
	return (m_slave);
}

size_t ebus::Telegram::getSlaveNN() const
{
	return (m_slaveNN);
}

std::byte ebus::Telegram::getSlaveCRC() const
{
	return (m_slaveCRC);
}

int ebus::Telegram::getSlaveState() const
{
	return (m_slaveState);
}

void ebus::Telegram::setMasterACK(const std::byte byte)
{
	m_masterACK = byte;
}

void ebus::Telegram::setType(const std::byte byte)
{
	if (byte == seq_broad)
		m_type = TEL_TYPE_BC;
	else if (isMaster(byte) == true)
		m_type = TEL_TYPE_MM;
	else
		m_type = TEL_TYPE_MS;
}

int ebus::Telegram::getType() const
{
	return (m_type);
}

bool ebus::Telegram::isValid() const
{
	if (m_type != TEL_TYPE_MS) return (m_masterState == SEQ_OK ? true : false);

	return ((m_masterState + m_slaveState) == SEQ_OK ? true : false);
}

const std::string ebus::Telegram::toString()
{
	std::ostringstream ostr;

	ostr << toStringMaster();

	if (m_masterState == SEQ_OK)
	{
		if (m_type == TEL_TYPE_MM) ostr << " " << toStringSlaveACK();

		if (m_type == TEL_TYPE_MS) ostr << " " << toStringSlave();
	}

	return (ostr.str());
}

const std::string ebus::Telegram::toStringMaster()
{
	std::ostringstream ostr;
	if (m_masterState != SEQ_OK)
		ostr << toStringMasterError();
	else
		ostr << m_master.toString();

	return (ostr.str());
}

const std::string ebus::Telegram::toStringMasterCRC()
{
	std::ostringstream ostr;
	if (m_masterState != SEQ_OK)
		ostr << toStringMasterError();
	else
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_masterCRC);

	return (ostr.str());
}

const std::string ebus::Telegram::toStringMasterACK() const
{
	std::ostringstream ostr;

	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_masterACK);

	return (ostr.str());
}

const std::string ebus::Telegram::toStringMasterError()
{
	std::ostringstream ostr;
	if (m_master.size() > 0) ostr << "'" << m_master.toString() << "' ";

	ostr << "master " << errorText(m_masterState);

	return (ostr.str());
}

const std::string ebus::Telegram::toStringSlave()
{
	std::ostringstream ostr;
	if (m_slaveState != SEQ_OK)
		ostr << toStringSlaveError();
	else
		ostr << m_slave.toString();

	return (ostr.str());
}

const std::string ebus::Telegram::toStringSlaveCRC()
{
	std::ostringstream ostr;
	if (m_slaveState != SEQ_OK)
		ostr << toStringSlaveError();
	else
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_slaveCRC);

	return (ostr.str());
}

const std::string ebus::Telegram::toStringSlaveACK() const
{
	std::ostringstream ostr;

	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_slaveACK);

	return (ostr.str());
}

const std::string ebus::Telegram::toStringSlaveError()
{
	std::ostringstream ostr;
	if (m_slave.size() > 0) ostr << "'" << m_slave.toString() << "' ";

	ostr << "slave " << errorText(m_slaveState);

	return (ostr.str());
}

const std::string ebus::Telegram::errorText(const int error)
{
	std::ostringstream ostr;

	ostr << SequenceErrors[error];

	return (ostr.str());
}

bool ebus::Telegram::isMaster(const std::byte byte)
{
	std::byte hi = (byte & std::byte(0xf0)) >> 4;
	std::byte lo = (byte & std::byte(0x0f));

	return (((hi == std::byte(0x0)) || (hi == std::byte(0x1)) || (hi == std::byte(0x3)) || (hi == std::byte(0x7))
		|| (hi == std::byte(0xf)))
		&& ((lo == std::byte(0x0)) || (lo == std::byte(0x1)) || (lo == std::byte(0x3)) || (lo == std::byte(0x7))
			|| (lo == std::byte(0xf))));
}

bool ebus::Telegram::isSlave(const std::byte byte)
{
	return (isMaster(byte) == false && byte != seq_syn && byte != seq_exp);
}

bool ebus::Telegram::isAddressValid(const std::byte byte)
{
	return (byte != seq_syn && byte != seq_exp);
}

std::byte ebus::Telegram::slaveAddress(const std::byte address)
{
	if (isSlave(address) == true) return (address);

	return (std::byte(std::to_integer<int>(address) + 5));
}

int ebus::Telegram::checkMasterSequence(Sequence &seq)
{
	// sequence is too short
	if (seq.size() < (size_t) 6) return (SEQ_ERR_SHORT);

	// source address is invalid
	if (isMaster(seq[0]) == false) return (SEQ_ERR_QQ);

	// target address is invalid
	if (isAddressValid(seq[1]) == false) return (SEQ_ERR_ZZ);

	// number data byte is invalid
	if (std::to_integer<int>(seq[4]) > seq_max_bytes) return (SEQ_ERR_NN);

	// sequence is too short (incl. CRC)
	if (seq.size() < (size_t) (5 + std::to_integer<int>(seq[4]) + 1)) return (SEQ_ERR_SHORT);

	return (SEQ_OK);
}

int ebus::Telegram::checkSlaveSequence(Sequence &seq)
{
	// sequence is too short
	if (seq.size() < (size_t) 2) return (SEQ_ERR_SHORT);

	// number data byte is invalid
	if (std::to_integer<int>(seq[0]) > seq_max_bytes) return (SEQ_ERR_NN);

	// sequence is too short (incl. CRC)
	if (seq.size() < (size_t) (1 + std::to_integer<int>(seq[0]) + 1)) return (SEQ_ERR_SHORT);

	return (SEQ_OK);
}
