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

#include <iostream>
#include <iomanip>
#include <cstdlib>

using std::stringstream;
using std::nouppercase;
using std::hex;
using std::setw;
using std::setfill;
using std::cout;
using std::endl;

//~ static const char* tty_black = "\033[1;30m";
//~ static const char* tty_red = "\033[1;31m";
//~ static const char* tty_green = "\033[1;32m";
//~ static const char* tty_yellow = "\033[1;33m";
//~ static const char* tty_blue = "\033[1;34m";
//~ static const char* tty_magenta = "\033[1;35m";
//~ static const char* tty_cyan = "\033[1;36m";
//~ static const char* tty_white = "\033[1;37m";
//~ static const char* tty_reset = "\033[0m";

#define TTY_RED   "\033[1;31m"
#define TTY_RESET "\033[0m"

static const char* SequenceTypeNames[] =
	{ "\033[1;34mBC\033[0m", "\033[1;36mMM\033[0m", "\033[1;35mMS\033[0m" };

EbusSequence::EbusSequence()
	: m_type()
{
}

EbusSequence::EbusSequence(const string& str)
{
	createMaster(str);
}

EbusSequence::EbusSequence(Sequence& seq)
{
	decodeUpdate(seq);
}

void EbusSequence::decodeUpdate(Sequence& seq)
{
	// sequence to short
	if (seq.size() < 6)
	{
		m_masterState = EBUS_ERR_SHORT;
		return;
	}

	// master sequence to long
	if ((int) seq[4] > 16)
	{
		m_masterState = EBUS_ERR_LONG;
		return;
	}

	setType(seq[1]);

	seq.reduce();

	m_master = Sequence(seq, 0, 5 + seq[4]);

	m_masterCRC = seq[5 + seq[4]];

	if (m_master.getCRC() != m_masterCRC) m_masterState = EBUS_WRN_CRC;

	if (m_type != st_Broadcast)
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

		// TODO handle NAK
	}

	if (m_type == st_MasterSlave)
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

		if (m_slave.getCRC() != m_slaveCRC) m_slaveState = EBUS_WRN_CRC;

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

		// TODO handle NAK
	}
}

void EbusSequence::createMaster(const string& str)
{
	m_masterState = EBUS_OK;
	Sequence seq;

	for (size_t i = 0; i + 1 < str.size(); i += 2)
	{
		unsigned long byte = strtoul(str.substr(i, 2).c_str(), NULL,
			16);
		seq.push_back((unsigned char) byte);
	}

	// sequence to short
	if (seq.size() < 5)
	{
		m_masterState = EBUS_ERR_SHORT;
		return;
	}

	// master sequence to long
	if ((int) seq[4] > 16)
	{
		m_masterState = EBUS_ERR_LONG;
		return;
	}

	setType(seq[1]);

	m_master = seq;

	m_masterCRC = seq.getCRC();
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
	if (seq[0] > 16)
	{
		m_slaveState = EBUS_ERR_LONG;
		return;
	}

	m_slave = Sequence(seq, 0, seq.size() - 1);
	m_slaveCRC = seq[seq.size() - 1];

	if (m_slave.getCRC() != m_slaveCRC) m_slaveState = EBUS_WRN_CRC;

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
		m_type = st_Broadcast;
	}
	else if (isMaster(byte) == true)
	{
		m_type = st_MasterMaster;
	}
	else
	{
		m_type = st_MasterSlave;
	}
}

SequenceType EbusSequence::getType() const
{
	return (m_type);
}

bool EbusSequence::isValid() const
{
	if (m_type != st_MasterSlave)
		return (m_masterState == EBUS_OK ? true : false);

	return ((m_masterState + m_slaveState) == EBUS_OK ? true : false);
}

const string EbusSequence::printUpdate()
{
	stringstream sstr;

	if (m_masterState < EBUS_OK) return (printMaster());

	sstr << SequenceTypeNames[m_type] << " " << printMaster();

	if (m_type == st_MasterMaster) sstr << " " << printMasterAck();

	if (m_type == st_MasterSlave && m_masterState >= EBUS_OK)
		sstr << " " << printSlave();

	return (sstr.str());
}

const string EbusSequence::printMaster()
{
	stringstream sstr;
	if (m_masterState >= EBUS_OK)
	{
		sstr << (m_masterState == EBUS_WRN_CRC ? TTY_RED : TTY_RESET)
			<< m_master.print() << TTY_RESET;
	}
	else
	{
		sstr << TTY_RED << "Master " << getErrorText(m_masterState)
			<< TTY_RESET;
	}

	return (sstr.str());
}

const string EbusSequence::printSlave()
{
	stringstream sstr;
	if (m_slaveState >= EBUS_OK)
	{
		sstr << (m_slaveState == EBUS_WRN_CRC ? TTY_RED : TTY_RESET)
			<< m_slave.print() << TTY_RESET;
	}
	else
	{
		sstr << TTY_RED << "Slave " << getErrorText(m_slaveState)
			<< TTY_RESET;
	}

	return (sstr.str());
}

const string EbusSequence::printMasterAck()
{
	stringstream sstr;
	if (m_masterState >= EBUS_OK)
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_masterACK);
	}
	else
	{
		sstr << TTY_RED << "Master " << getErrorText(m_masterState)
			<< TTY_RESET;
	}

	return (sstr.str());
}

const string EbusSequence::printSlaveAck()
{
	stringstream sstr;
	if (m_slaveState >= EBUS_OK)
	{
		sstr << nouppercase << hex << setw(2) << setfill('0')
			<< static_cast<unsigned>(m_slaveACK);
	}
	else
	{
		sstr << TTY_RED << "Slave " << getErrorText(m_slaveState)
			<< TTY_RESET;
	}

	return (sstr.str());
}

const string EbusSequence::getErrorText(const int error)
{
	stringstream result;

	switch (error)
	{
	case EBUS_WRN_CRC:
		result << "sequence CRC error";
		break;
	case EBUS_ERR_SHORT:
		result << "sequence to short";
		break;
	case EBUS_ERR_LONG:
		result << "sequence to long";
		break;
	case EBUS_ERR_ACK:
		result << "sequence ACK error";
		break;
	default:
		result << "unknown error code";
		break;
	}

	return (result.str());
}
