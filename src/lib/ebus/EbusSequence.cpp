/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebuscpp.
 *
 * ebuscpp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebuscpp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebuscpp. If not, see http://www.gnu.org/licenses/.
 */

#include "EbusSequence.h"
#include "Common.h"
#include "Color.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <map>

using libebus::Sequence;
using std::ostringstream;
using std::nouppercase;
using std::hex;
using std::setw;
using std::setfill;
using std::cout;
using std::endl;
using std::map;

map<int, string> SequenceErrors =
{
{ EBUS_EMPTY, "is empty" },

{ EBUS_ERR_SHORT, "sequence to short" },
{ EBUS_ERR_LONG, "sequence to long" },
{ EBUS_ERR_BYTES, "sequence to much data bytes" },
{ EBUS_ERR_CRC, "sequence CRC error" },
{ EBUS_ERR_ACK, "sequence ACK error" },
{ EBUS_ERR_MASTER, "wrong master address" },
{ EBUS_ERR_SLAVE, "wrong slave address" }, };

libebus::EbusSequence::EbusSequence()
{
}

libebus::EbusSequence::EbusSequence(Sequence& seq)
{
	seq.reduce();
	int offset = 0;

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

	Sequence master(seq, 0, 5 + seq[4]);
	createMaster(master);

	if (m_masterState != EBUS_OK) return;

	if (m_type != EBUS_TYPE_BC)
	{
		m_slaveACK = seq[5 + m_masterNN + 1];
		if (m_slaveACK != ACK && m_slaveACK != NAK) m_slaveState =
		EBUS_ERR_ACK;

		// handle NAK from slave
		if (m_slaveACK == NAK)
		{
			offset = master.size() + 2;
			m_master.clear();

			Sequence tmp(seq, offset, 5 + seq[offset + 4]);
			createMaster(tmp);

			if (m_masterState != EBUS_OK) return;

			m_slaveACK = seq[offset + 5 + m_masterNN + 1];
			if (m_slaveACK != ACK && m_slaveACK != NAK) m_slaveState =
			EBUS_ERR_ACK;
		}
	}

	if (m_type == EBUS_TYPE_MS)
	{
		Sequence slave(seq, offset + 5 + m_masterNN + 2, 1 + seq[offset + 5 + m_masterNN + 2] + 1);
		createSlave(slave);

		m_masterACK = seq[(offset + 5 + m_masterNN + 3 + m_slaveNN + 1)];
		if (m_masterACK != ACK && m_masterACK != NAK) m_masterState =
		EBUS_ERR_ACK;

		// handle NAK from master
		if (m_masterACK == NAK)
		{
			offset += slave.size() + 2;
			m_slave.clear();

			Sequence tmp(seq, offset + 5 + m_masterNN + 2, 1 + seq[offset + 5 + m_masterNN + 2] + 1);
			createSlave(tmp);

			m_masterACK = seq[(offset + 5 + m_masterNN + 3 + m_slaveNN + 1)];
			if (m_masterACK != ACK && m_masterACK != NAK) m_masterState =
			EBUS_ERR_ACK;
		}
	}
}

void libebus::EbusSequence::createMaster(const unsigned char source, const unsigned char target, const string& str)
{
	ostringstream ostr;
	ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(source);
	ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(target);
	ostr << nouppercase << setw(0) << str;
	createMaster(ostr.str());
}

void libebus::EbusSequence::createMaster(const unsigned char source, const string& str)
{
	ostringstream ostr;
	ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(source);
	ostr << nouppercase << setw(0) << str;
	createMaster(ostr.str());
}

void libebus::EbusSequence::createMaster(const string& str)
{
	Sequence seq(str);
	seq.reduce();
	createMaster(seq);
}

void libebus::EbusSequence::createMaster(Sequence& seq)
{
	m_masterState = EBUS_OK;

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

	// wrong master address
	if (isMaster(seq[0]) == false)
	{
		m_masterState = EBUS_ERR_MASTER;
		return;
	}

	// wrong slave address
	if (isValidAddress(seq[1]) == false)
	{
		m_masterState = EBUS_ERR_SLAVE;
		return;
	}

	m_masterQQ = seq[0];
	m_masterZZ = seq[1];
	setType(seq[1]);
	m_masterNN = seq[4];

	if (seq.size() == (5 + m_masterNN))
	{
		m_master = seq;
		m_masterCRC = seq.getCRC();
	}
	else
	{
		m_master = Sequence(seq, 0, 5 + m_masterNN);
		m_masterCRC = seq[5 + m_masterNN];

		if (m_master.getCRC() != m_masterCRC) m_masterState =
		EBUS_ERR_CRC;
	}
}

void libebus::EbusSequence::createSlave(const string& str)
{
	Sequence seq(str);
	seq.reduce();
	createSlave(seq);
}

void libebus::EbusSequence::createSlave(Sequence& seq)
{
	m_slaveState = EBUS_OK;

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

		if (m_slave.getCRC() != m_slaveCRC) m_slaveState = EBUS_ERR_CRC;
	}
}

void libebus::EbusSequence::clear()
{
	m_type = -1;

	m_masterQQ = 0;
	m_masterZZ = 0;

	m_master.clear();
	m_masterNN = 0;
	m_masterCRC = 0;
	m_masterACK = 0;
	m_masterState = EBUS_EMPTY;

	m_slave.clear();
	m_slaveNN = 0;
	m_slaveCRC = 0;
	m_slaveACK = 0;
	m_slaveState = EBUS_EMPTY;
}

unsigned char libebus::EbusSequence::getMasterQQ() const
{
	return (m_masterQQ);
}

unsigned char libebus::EbusSequence::getMasterZZ() const
{
	return (m_masterZZ);
}

Sequence libebus::EbusSequence::getMaster() const
{
	return (m_master);
}

size_t libebus::EbusSequence::getMasterNN() const
{
	return (m_masterNN);
}

unsigned char libebus::EbusSequence::getMasterCRC() const
{
	return (m_masterCRC);
}

int libebus::EbusSequence::getMasterState() const
{
	return (m_masterState);
}

void libebus::EbusSequence::setMasterACK(const unsigned char byte)
{
	m_masterACK = byte;
}

Sequence libebus::EbusSequence::getSlave() const
{
	return (m_slave);
}

size_t libebus::EbusSequence::getSlaveNN() const
{
	return (m_slaveNN);
}

unsigned char libebus::EbusSequence::getSlaveCRC() const
{
	return (m_slaveCRC);
}

int libebus::EbusSequence::getSlaveState() const
{
	return (m_slaveState);
}

void libebus::EbusSequence::setSlaveACK(const unsigned char byte)
{
	m_slaveACK = byte;
}

void libebus::EbusSequence::setType(const unsigned char byte)
{
	if (byte == BROADCAST)
		m_type = EBUS_TYPE_BC;
	else if (isMaster(byte) == true)
		m_type = EBUS_TYPE_MM;
	else
		m_type = EBUS_TYPE_MS;
}

int libebus::EbusSequence::getType() const
{
	return (m_type);
}

bool libebus::EbusSequence::isValid() const
{
	if (m_type != EBUS_TYPE_MS) return (m_masterState == EBUS_OK ? true : false);

	return ((m_masterState + m_slaveState) == EBUS_OK ? true : false);
}

const string libebus::EbusSequence::toString()
{
	ostringstream ostr;

	ostr << toStringMaster();

	if (m_type == EBUS_TYPE_MM) ostr << " " << toStringMasterACK();

	if (m_type == EBUS_TYPE_MS) ostr << " " << toStringSlave();

	return (ostr.str());
}

const string libebus::EbusSequence::toStringLog()
{
	ostringstream ostr;

	if (m_masterState != EBUS_OK) return (toStringMaster());

	if (m_type == EBUS_TYPE_BC)
		ostr << libutils::color::blue << "BC" << libutils::color::reset << " " << toStringMaster();
	else if (m_type == EBUS_TYPE_MM)
		ostr << libutils::color::cyan << "MM" << libutils::color::reset << " " << toStringMaster();
	else
		ostr << libutils::color::magenta << "MS" << libutils::color::reset << " " << toStringMaster();

	if (m_type == EBUS_TYPE_MM) ostr << " " << toStringMasterACK();

	if (m_type == EBUS_TYPE_MS) ostr << " " << toStringSlave();

	return (ostr.str());
}

const string libebus::EbusSequence::toStringMaster()
{
	ostringstream ostr;
	if (m_masterState != EBUS_OK)
		ostr << libutils::color::red << m_master.toString() << " Master " << errorText(m_masterState)
			<< libutils::color::reset;
	else
		ostr << m_master.toString();

	return (ostr.str());
}

const string libebus::EbusSequence::toStringMasterCRC()
{
	ostringstream ostr;
	if (m_masterState != EBUS_OK)
		ostr << libutils::color::red << m_master.toString() << " Master " << errorText(m_masterState)
			<< libutils::color::reset;
	else
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_masterCRC);

	return (ostr.str());
}

const string libebus::EbusSequence::toStringMasterACK()
{
	ostringstream ostr;
	if (m_masterState != EBUS_OK)
		ostr << libutils::color::red << m_master.toString() << " Master " << errorText(m_masterState)
			<< libutils::color::reset;
	else
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_masterACK);

	return (ostr.str());
}

const string libebus::EbusSequence::toStringSlave()
{
	ostringstream ostr;
	if (m_slaveState != EBUS_OK)
		ostr << libutils::color::red << m_slave.toString() << " Slave " << errorText(m_slaveState)
			<< libutils::color::reset;
	else
		ostr << m_slave.toString();

	return (ostr.str());
}

const string libebus::EbusSequence::toStringSlaveCRC()
{
	ostringstream ostr;
	if (m_slaveState != EBUS_OK)
		ostr << libutils::color::red << m_slave.toString() << " Slave " << errorText(m_slaveState)
			<< libutils::color::reset;
	else
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_slaveCRC);

	return (ostr.str());
}

const string libebus::EbusSequence::toStringSlaveACK()
{
	ostringstream ostr;
	if (m_slaveState != EBUS_OK)
		ostr << libutils::color::red << m_slave.toString() << " Slave " << errorText(m_slaveState)
			<< libutils::color::reset;
	else
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_slaveACK);

	return (ostr.str());
}

const string libebus::EbusSequence::errorText(const int error) const
{
	ostringstream errStr;

	errStr << SequenceErrors[error];

	return (errStr.str());
}
