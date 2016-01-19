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

#ifndef EBUS_EBUSSEQUENCE_H
#define EBUS_EBUSSEQUENCE_H

#include "Sequence.h"

#define EBUS_WRN_CRC     1 // crc differs
#define EBUS_OK          0 // success
#define EBUS_ERR_SHORT  -1 // sequence to short
#define EBUS_ERR_LONG   -2 // sequence to long
#define EBUS_ERR_ACK    -3 // ack byte wrong

enum SequenceType
{
	st_Broadcast = 0, st_MasterMaster, st_MasterSlave
};

class EbusSequence
{

public:
	EbusSequence();
	explicit EbusSequence(const string& str);
	explicit EbusSequence(Sequence& seq);

	void decodeUpdate(Sequence& seq);

	void createMaster(const string& str);
	void createSlave(Sequence& seq);

	void clear();

	Sequence getMaster() const;
	unsigned char getMasterCRC() const;
	int getMasterState() const;

	Sequence getSlave() const;
	unsigned char getSlaveCRC() const;
	int getSlaveState() const;

	void setType(const unsigned char& byte);
	SequenceType getType() const;

	bool isValid() const;

	const string printUpdate();
	const string printMaster();
	const string printSlave();
	const string printMasterAck();
	const string printSlaveAck();

private:
	SequenceType m_type;

	Sequence m_master;
	unsigned char m_masterCRC = 0;
	unsigned char m_masterACK = 0;
	int m_masterState = EBUS_OK;

	Sequence m_slave;
	unsigned char m_slaveCRC = 0;
	unsigned char m_slaveACK = 0;
	int m_slaveState = EBUS_OK;

	static const string getErrorText(const int error);

};

#endif // EBUS_EBUSSEQUENCE_H

