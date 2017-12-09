/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#include <EbusSequence.h>
#include <EbusCommon.h>

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <map>

std::map<int, std::string> SequenceErrors =
{
{ SEQ_TRANSMIT, "sequence sending failed" },
{ SEQ_EMPTY, "sequence is empty" },

{ SEQ_ERR_SHORT, "sequence is too short" },
{ SEQ_ERR_LONG, "sequence is too long" },
{ SEQ_ERR_NN, "data byte number is invalid" },
{ SEQ_ERR_CRC, "sequence has a CRC error" },
{ SEQ_ERR_ACK, "acknowledge byte is invalid" },
{ SEQ_ERR_QQ, "source address is invalid" },
{ SEQ_ERR_ZZ, "target address is invalid" },
{ SEQ_ERR_ACK_MISS, "acknowledge byte is missing" },};

ebusfsm::EbusSequence::EbusSequence()
{
}

ebusfsm::EbusSequence::EbusSequence(Sequence& seq)
{
	parseSequence(seq);
}

void ebusfsm::EbusSequence::parseSequence(Sequence& seq)
{
	seq.reduce();
	int offset = 0;

	m_masterState = checkMasterSequence(seq);

	if (m_masterState != SEQ_OK) return;

	Sequence master(seq, 0, 5 + seq[4] + 1);
	createMaster(master);

	if (m_masterState != SEQ_OK) return;

	if (m_type != SEQ_TYPE_BC)
	{
		// acknowledge byte is missing
		if (seq.size() <= (size_t) (5 + m_masterNN + 1))
		{
			m_slaveState = SEQ_ERR_ACK_MISS;
			return;
		}

		m_slaveACK = seq[5 + m_masterNN + 1];

		// acknowledge byte is invalid
		if (m_slaveACK != SEQ_ACK && m_slaveACK != SEQ_NAK)
		{
			m_slaveState = SEQ_ERR_ACK;
			return;
		}

		// handle NAK from slave
		if (m_slaveACK == SEQ_NAK)
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

			Sequence master2(tmp, 0, 5 + tmp[4] + 1);
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
			if (m_slaveACK != SEQ_ACK && m_slaveACK != SEQ_NAK)
			{
				m_slaveState = SEQ_ERR_ACK;
				return;
			}

			// acknowledge byte is negativ
			if (m_slaveACK == SEQ_NAK)
			{
				// sequence is too long
				if (tmp.size() > (size_t) (5 + m_masterNN + 2))
					m_masterState = SEQ_ERR_LONG;

				// sequence sending failed
				else
					m_masterState = SEQ_TRANSMIT;

				return;
			}
		}
	}

	if (m_type == SEQ_TYPE_MS)
	{
		offset += 5 + m_masterNN + 2;

		Sequence seq2(seq, offset);
		m_slaveState = checkSlaveSequence(seq2);

		if (m_slaveState != SEQ_OK) return;

		Sequence slave(seq2, 0, 1 + seq2[0] + 1);
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
		if (m_masterACK != SEQ_ACK && m_masterACK != SEQ_NAK)
		{
			m_masterState = SEQ_ERR_ACK;
			return;
		}

		// handle NAK from master
		if (m_masterACK == SEQ_NAK)
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

			Sequence slave2(seq2, offset, 1 + seq2[offset] + 1);
			createSlave(slave2);

			// acknowledge byte is missing
			if (tmp.size() <= (size_t) (1 + m_slaveNN + 1))
			{
				m_masterState = SEQ_ERR_ACK_MISS;
				return;
			}

			m_masterACK = tmp[1 + m_slaveNN + 1];

			// acknowledge byte is invalid
			if (m_masterACK != SEQ_ACK && m_masterACK != SEQ_NAK)
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
			if (m_masterACK == SEQ_NAK)
			{
				// sequence sending failed
				m_slaveState = SEQ_TRANSMIT;
				return;
			}

		}
	}
}

void ebusfsm::EbusSequence::createMaster(const unsigned char source, const unsigned char target, const std::string& str)
{
	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(source);
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(target);
	ostr << std::nouppercase << std::setw(0) << str;
	createMaster(ostr.str());
}

void ebusfsm::EbusSequence::createMaster(const unsigned char source, const std::string& str)
{
	std::ostringstream ostr;
	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(source);
	ostr << std::nouppercase << std::setw(0) << str;
	createMaster(ostr.str());
}

void ebusfsm::EbusSequence::createMaster(const std::string& str)
{
	Sequence seq(str);
	createMaster(seq);
}

void ebusfsm::EbusSequence::createMaster(Sequence& seq)
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

	// data byte number is invalid
	if (seq[4] > SEQ_NN_MAX)
	{
		m_masterState = SEQ_ERR_NN;
		return;
	}

	// sequence is too short (excl. CRC)
	if (seq.size() < (size_t) (5 + seq[4]))
	{
		m_masterState = SEQ_ERR_SHORT;
		return;
	}

	// sequence is too long (incl. CRC)
	if (seq.size() > (size_t) (5 + seq[4] + 1))
	{
		m_masterState = SEQ_ERR_LONG;
		return;
	}

	m_masterQQ = seq[0];
	m_masterZZ = seq[1];
	setType(seq[1]);
	m_masterNN = seq[4];

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

void ebusfsm::EbusSequence::createSlave(const std::string& str)
{
	Sequence seq(str);
	createSlave(seq);
}

void ebusfsm::EbusSequence::createSlave(Sequence& seq)
{
	m_slaveState = SEQ_OK;
	seq.reduce();

	// sequence is too short
	if (seq.size() < (size_t)2)
	{
		m_slaveState = SEQ_ERR_SHORT;
		return;
	}

	// data byte number is invalid
	if (seq[0] > SEQ_NN_MAX)
	{
		m_slaveState = SEQ_ERR_NN;
		return;
	}

	// sequence is too short (excl. CRC)
	if (seq.size() < (size_t) (1 + seq[0]))
	{
		m_slaveState = SEQ_ERR_SHORT;
		return;
	}

	// sequence is too long (incl. CRC)
	if (seq.size() > (size_t) (1 + seq[0] + 1))
	{
		m_slaveState = SEQ_ERR_LONG;
		return;
	}

	m_slaveNN = seq[0];

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

void ebusfsm::EbusSequence::clear()
{
	m_type = -1;

	m_masterQQ = 0;
	m_masterZZ = 0;

	m_master.clear();
	m_masterNN = 0;
	m_masterCRC = 0;
	m_masterACK = 0;
	m_masterState = SEQ_EMPTY;

	m_slave.clear();
	m_slaveNN = 0;
	m_slaveCRC = 0;
	m_slaveACK = 0;
	m_slaveState = SEQ_EMPTY;
}

unsigned char ebusfsm::EbusSequence::getMasterQQ() const
{
	return (m_masterQQ);
}

unsigned char ebusfsm::EbusSequence::getMasterZZ() const
{
	return (m_masterZZ);
}

ebusfsm::Sequence ebusfsm::EbusSequence::getMaster() const
{
	return (m_master);
}

size_t ebusfsm::EbusSequence::getMasterNN() const
{
	return (m_masterNN);
}

unsigned char ebusfsm::EbusSequence::getMasterCRC() const
{
	return (m_masterCRC);
}

int ebusfsm::EbusSequence::getMasterState() const
{
	return (m_masterState);
}

void ebusfsm::EbusSequence::setSlaveACK(const unsigned char byte)
{
	m_slaveACK = byte;
}

ebusfsm::Sequence ebusfsm::EbusSequence::getSlave() const
{
	return (m_slave);
}

size_t ebusfsm::EbusSequence::getSlaveNN() const
{
	return (m_slaveNN);
}

unsigned char ebusfsm::EbusSequence::getSlaveCRC() const
{
	return (m_slaveCRC);
}

int ebusfsm::EbusSequence::getSlaveState() const
{
	return (m_slaveState);
}

void ebusfsm::EbusSequence::setMasterACK(const unsigned char byte)
{
	m_masterACK = byte;
}

void ebusfsm::EbusSequence::setType(const unsigned char byte)
{
	if (byte == SEQ_BROAD)
		m_type = SEQ_TYPE_BC;
	else if (isMaster(byte) == true)
		m_type = SEQ_TYPE_MM;
	else
		m_type = SEQ_TYPE_MS;
}

int ebusfsm::EbusSequence::getType() const
{
	return (m_type);
}

bool ebusfsm::EbusSequence::isValid() const
{
	if (m_type != SEQ_TYPE_MS) return (m_masterState == SEQ_OK ? true : false);

	return ((m_masterState + m_slaveState) == SEQ_OK ? true : false);
}

const std::string ebusfsm::EbusSequence::toString()
{
	std::ostringstream ostr;

	ostr << toStringMaster();

	if (m_masterState == SEQ_OK)
	{
		if (m_type == SEQ_TYPE_MM) ostr << " " << toStringSlaveACK();

		if (m_type == SEQ_TYPE_MS) ostr << " " << toStringSlave();
	}

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringMaster()
{
	std::ostringstream ostr;
	if (m_masterState != SEQ_OK)
		ostr << toStringMasterError();
	else
		ostr << m_master.toString();

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringMasterCRC()
{
	std::ostringstream ostr;
	if (m_masterState != SEQ_OK)
		ostr << toStringMasterError();
	else
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_masterCRC);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringMasterACK() const
{
	std::ostringstream ostr;

	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_masterACK);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringMasterError()
{
	std::ostringstream ostr;
	if (m_master.size() > 0) ostr << "'" << m_master.toString() << "' ";

	ostr << "The master " << errorText(m_masterState);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringSlave()
{
	std::ostringstream ostr;
	if (m_slaveState != SEQ_OK)
		ostr << toStringSlaveError();
	else
		ostr << m_slave.toString();

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringSlaveCRC()
{
	std::ostringstream ostr;
	if (m_slaveState != SEQ_OK)
		ostr << toStringSlaveError();
	else
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_slaveCRC);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringSlaveACK() const
{
	std::ostringstream ostr;

	ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_slaveACK);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::toStringSlaveError()
{
	std::ostringstream ostr;
	if (m_slave.size() > 0) ostr << "'" << m_slave.toString() << "' ";

	ostr << "The slave " << errorText(m_slaveState);

	return (ostr.str());
}

const std::string ebusfsm::EbusSequence::errorText(const int error)
{
	std::ostringstream ostr;

	ostr << SequenceErrors[error];

	return (ostr.str());
}

int ebusfsm::EbusSequence::checkMasterSequence(Sequence& seq)
{
	// sequence is too short
	if (seq.size() < (size_t)6) return (SEQ_ERR_SHORT);

	// source address is invalid
	if (isMaster(seq[0]) == false) return (SEQ_ERR_QQ);

	// target address is invalid
	if (isAddressValid(seq[1]) == false) return (SEQ_ERR_ZZ);

	// data byte number is invalid
	if (seq[4] > SEQ_NN_MAX) return (SEQ_ERR_NN);

	// sequence is too short (incl. CRC)
	if (seq.size() < (size_t) (5 + seq[4] + 1)) return (SEQ_ERR_SHORT);

	return (SEQ_OK);
}

int ebusfsm::EbusSequence::checkSlaveSequence(Sequence& seq)
{
	// sequence is too short
	if (seq.size() < (size_t)2) return (SEQ_ERR_SHORT);

	// data byte number is invalid
	if (seq[0] > SEQ_NN_MAX) return (SEQ_ERR_NN);

	// sequence is too short (incl. CRC)
	if (seq.size() < (size_t) (1 + seq[0] + 1)) return (SEQ_ERR_SHORT);

	return (SEQ_OK);
}
