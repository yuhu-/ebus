/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
 *
 * This file is part of ebusgate.
 *
 * ebusgate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusgate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusgate. If not, see http://www.gnu.org/licenses/.
 */

#include "EbusSequence.h"
#include "Common.h"
#include "Color.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <map>

using std::stringstream;
using std::nouppercase;
using std::hex;
using std::setw;
using std::setfill;
using std::cout;
using std::endl;
using std::map;

map<int, string> SequenceErrors =
{
{ EBUS_ERR_SHORT, "sequence to short" },
{ EBUS_ERR_LONG, "sequence to long" },
{ EBUS_ERR_BYTES, "sequence to much data bytes" },
{ EBUS_ERR_CRC, "sequence CRC error" },
{ EBUS_ERR_ACK, "sequence ACK error" } };

EbusSequence::EbusSequence()
{
}

EbusSequence::EbusSequence(Sequence& seq)
{
	decodeSequence(seq);
}

//TODO implement better decodeSequence
void EbusSequence::decodeSequence(Sequence& seq)
{
	seq.reduce();

	// sequence to short
	if (seq.size() < 5)
	{
		m_masterState = EBUS_ERR_SHORT;
		return;
	}

	// to much data bytes
	if ((int) seq[4] > 16)
	{
		m_masterState = EBUS_ERR_BYTES;
		return;
	}

	setType(seq[1]);

	m_master = Sequence(seq, 0, 5 + seq[4]);

	m_masterCRC = seq[5 + seq[4]];

	if (m_master.getCRC() != m_masterCRC) m_masterState = EBUS_ERR_CRC;

	if (m_type != EBUS_TYPE_BC)
	{
		m_slaveACK = seq[5 + seq[4] + 1];

		switch (m_slaveACK)
		{
		case ACK:
		case NAK:
			break;
		default:
			m_slaveState = EBUS_ERR_ACK;
			return;
		}
	}

	if (m_type == EBUS_TYPE_MS)
	{
		// slave sequence to long
		if (1 + seq[5 + seq[4] + 2] > 16)
		{
			m_slaveState = EBUS_ERR_LONG;
			return;
		}

		m_slave = Sequence(seq, 5 + seq[4] + 2,
			1 + seq[5 + seq[4] + 2]);

		m_slaveCRC = seq[5 + seq[4] + 2 + 1 + seq[5 + seq[4] + 2]];

		if (m_slave.getCRC() != m_slaveCRC) m_slaveState = EBUS_ERR_CRC;

		m_masterACK = seq[5 + seq[4] + 2 + 1 + seq[5 + seq[4] + 2] + 1];

		switch (m_masterACK)
		{
		case ACK:
		case NAK:
			break;
		default:
			m_masterState = EBUS_ERR_ACK;
			return;
		}
	}
}

void EbusSequence::createMaster(const string& str)
{
	Sequence seq(str);

	createMaster(seq);
}

void EbusSequence::createMaster(Sequence& seq)
{
	m_masterState = EBUS_OK;

	seq.reduce();

	// sequence to short
	if (seq.size() < 5)
	{
		m_masterState = EBUS_ERR_SHORT;
		return;
	}

	// master sequence to long
	if (seq.size() > (size_t) (5 + seq[4] + 1))
	{
		m_masterState = EBUS_ERR_LONG;
		return;
	}

	// to much data bytes
	if ((int) seq[4] > 16)
	{
		m_masterState = EBUS_ERR_BYTES;
		return;
	}

	setType(seq[1]);

	if (seq.size() == (size_t) (5 + seq[4]))
	{
		m_master = seq;
		m_masterCRC = seq.getCRC();
	}
	else
	{
		m_master = Sequence(seq, 0, 5 + seq[4]);
		m_masterCRC = seq[5 + seq[4]];

		if (m_master.getCRC() != m_masterCRC) m_masterState =
		EBUS_ERR_CRC;
	}
}

void EbusSequence::createSlave(const string& str)
{
	Sequence seq(str);

	createSlave(seq);
}

void EbusSequence::createSlave(Sequence& seq)
{
	m_slaveState = EBUS_OK;

	seq.reduce();

	// sequence to short
	if (seq.size() < 2)
	{
		m_slaveState = EBUS_ERR_SHORT;
		return;
	}

	// slave sequence to long
	if (seq.size() > (size_t) (1 + seq[0] + 1))
	{
		m_slaveState = EBUS_ERR_LONG;
		return;
	}

	// to much data bytes
	if (seq[0] > 16)
	{
		m_slaveState = EBUS_ERR_BYTES;
		return;
	}

	if (seq.size() == (size_t) (1 + seq[0]))
	{
		m_slave = seq;
		m_slaveCRC = seq.getCRC();
	}
	else
	{
		m_slave = Sequence(seq, 0, 1 + seq[0]);
		m_slaveCRC = seq[1 + seq[0]];

		if (m_slave.getCRC() != m_slaveCRC) m_slaveState = EBUS_ERR_CRC;
	}
}

void EbusSequence::clear()
{
	m_master.clear();
	m_masterCRC = 0;
	m_masterACK = 0;
	m_masterState = EBUS_OK;

	m_slave.clear();
	m_slaveCRC = 0;
	m_slaveACK = 0;
	m_slaveState = EBUS_OK;
}

Sequence EbusSequence::getMaster() const
{
	return (m_master);
}

unsigned char EbusSequence::getMasterCRC() const
{
	return (m_masterCRC);
}

int EbusSequence::getMasterState() const
{
	return (m_masterState);
}

Sequence EbusSequence::getSlave() const
{
	return (m_slave);
}

unsigned char EbusSequence::getSlaveCRC() const
{
	return (m_slaveCRC);
}

int EbusSequence::getSlaveState() const
{
	return (m_slaveState);
}

void EbusSequence::setType(const unsigned char& byte)
{
	if (byte == BROADCAST)
	{
		m_type = EBUS_TYPE_BC;
	}
	else if (isMaster(byte) == true)
	{
		m_type = EBUS_TYPE_MM;
	}
	else
	{
		m_type = EBUS_TYPE_MS;
	}
}

int EbusSequence::getType() const
{
	return (m_type);
}

bool EbusSequence::isValid() const
{
	if (m_type != EBUS_TYPE_MS)
		return (m_masterState == EBUS_OK ? true : false);

	return ((m_masterState + m_slaveState) == EBUS_OK ? true : false);
}

const string EbusSequence::toStringFull()
{
	stringstream sstr;

	if (m_masterState != EBUS_OK) return (toStringMaster());

	if (m_type == EBUS_TYPE_BC)
	{
		sstr << color::blue << "BC " << color::reset
			<< toStringMaster();
	}
	else if (m_type == EBUS_TYPE_MM)
	{
		sstr << color::cyan << "MM " << color::reset
			<< toStringMaster();
	}
	else
	{
		sstr << color::magenta << "MS " << color::reset
			<< toStringMaster();
	}

	if (m_type == EBUS_TYPE_MM) sstr << " " << toStringMasterACK();

	if (m_type == EBUS_TYPE_MS) sstr << " " << toStringSlave();

	return (sstr.str());
}

const string EbusSequence::toStringMaster()
{
	stringstream sstr;
	if (m_masterState != EBUS_OK)
	{
		sstr << color::red << m_master.toString() << "Master "
			<< errorText(m_masterState) << color::reset;
	}
	else
	{
		sstr << m_master.toString();
	}

	return (sstr.str());
}

const string EbusSequence::toStringMasterCRC()
{
	stringstream sstr;
	if (m_masterState != EBUS_OK)
	{
		sstr << color::red << m_master.toString() << "Master "
			<< errorText(m_masterState) << color::reset;
	}
	else
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_masterCRC);
	}

	return (sstr.str());
}

const string EbusSequence::toStringMasterACK()
{
	stringstream sstr;
	if (m_masterState != EBUS_OK)
	{
		sstr << color::red << m_master.toString() << "Master "
			<< errorText(m_masterState) << color::reset;
	}
	else
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_masterACK);
	}

	return (sstr.str());
}

const string EbusSequence::toStringSlave()
{
	stringstream sstr;
	if (m_slaveState != EBUS_OK)
	{
		sstr << color::red << m_slave.toString() << "Slave "
			<< errorText(m_slaveState) << color::reset;
	}
	else
	{
		sstr << m_slave.toString();
	}
	return (sstr.str());
}

const string EbusSequence::toStringSlaveCRC()
{
	stringstream sstr;
	if (m_slaveState != EBUS_OK)
	{
		sstr << color::red << m_slave.toString() << "Slave "
			<< errorText(m_slaveState) << color::reset;
	}
	else
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_slaveCRC);
	}

	return (sstr.str());
}

const string EbusSequence::toStringSlaveACK()
{
	stringstream sstr;
	if (m_slaveState != EBUS_OK)
	{
		sstr << color::red << m_slave.toString() << "Slave "
			<< errorText(m_slaveState) << color::reset;
	}
	else
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_slaveACK);
	}

	return (sstr.str());
}

const string EbusSequence::errorText(const int error)
{
	stringstream result;

	result << SequenceErrors[error];

	return (result.str());
}
