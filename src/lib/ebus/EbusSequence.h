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

#ifndef LIBEBUS_EBUSSEQUENCE_H
#define LIBEBUS_EBUSSEQUENCE_H

#include "Sequence.h"

namespace libebus
{

#define SEQ_EMPTY       1 // sequence is empty

#define SEQ_OK          0 // success

#define SEQ_ERR_SHORT  -1 // sequence to short
#define SEQ_ERR_LONG   -2 // sequence to long
#define SEQ_ERR_BYTES  -3 // sequence to much data bytes
#define SEQ_ERR_CRC    -4 // sequence CRC error
#define SEQ_ERR_ACK    -5 // sequence ACK error
#define SEQ_ERR_MASTER -6 // wrong master address
#define SEQ_ERR_SLAVE  -7 // wrong slave address

#define SEQ_TYPE_BC     0 // broadcast
#define SEQ_TYPE_MM     1 // master master
#define SEQ_TYPE_MS     2 // master slave

#define SEQ_ACK      0x00 // positive acknowledge
#define SEQ_NAK      0xff // negative acknowledge
#define SEQ_BROAD    0xfe // broadcast destination address

class EbusSequence
{

public:
	EbusSequence();
	explicit EbusSequence(Sequence& seq);

	void createMaster(const unsigned char source, const unsigned char target, const string& str);
	void createMaster(const unsigned char source, const string& str);
	void createMaster(const string& str);
	void createMaster(Sequence& seq);

	void createSlave(const string& str);
	void createSlave(Sequence& seq);

	void clear();

	unsigned char getMasterQQ() const;
	unsigned char getMasterZZ() const;

	Sequence getMaster() const;
	size_t getMasterNN() const;
	unsigned char getMasterCRC() const;
	int getMasterState() const;

	void setSlaveACK(const unsigned char byte);

	Sequence getSlave() const;
	size_t getSlaveNN() const;
	unsigned char getSlaveCRC() const;
	int getSlaveState() const;

	void setMasterACK(const unsigned char byte);

	void setType(const unsigned char byte);
	int getType() const;

	bool isValid() const;

	const string toString();

	const string toStringMaster();
	const string toStringMasterCRC();
	const string toStringMasterACK();
	const string toStringMasterError();

	const string toStringSlave();
	const string toStringSlaveCRC();
	const string toStringSlaveACK();
	const string toStringSlaveError();

	static const string errorText(const int error);

private:
	int m_type = -1;

	unsigned char m_masterQQ = 0;
	unsigned char m_masterZZ = 0;

	Sequence m_master;
	size_t m_masterNN = 0;
	unsigned char m_masterCRC = 0;
	int m_masterState = SEQ_EMPTY;

	unsigned char m_slaveACK = 0;

	Sequence m_slave;
	size_t m_slaveNN = 0;
	unsigned char m_slaveCRC = 0;
	int m_slaveState = SEQ_EMPTY;

	unsigned char m_masterACK = 0;
};

} // namespace libebus

#endif // LIBEBUS_EBUSSEQUENCE_H

