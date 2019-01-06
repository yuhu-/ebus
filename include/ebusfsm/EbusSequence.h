/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#ifndef EBUSFSM_EBUSSEQUENCE_H
#define EBUSFSM_EBUSSEQUENCE_H

#include <Sequence.h>

namespace ebusfsm
{

#define SEQ_TRANSMIT       2 // sequence sending failed
#define SEQ_EMPTY          1 // sequence is empty

#define SEQ_OK             0 // success

#define SEQ_ERR_SHORT     -1 // sequence is too short
#define SEQ_ERR_LONG      -2 // sequence is too long
#define SEQ_ERR_NN        -3 // data byte number is invalid
#define SEQ_ERR_CRC       -4 // sequence has a CRC error
#define SEQ_ERR_ACK       -5 // acknowledge byte is invalid
#define SEQ_ERR_QQ        -6 // source address is invalid
#define SEQ_ERR_ZZ        -7 // target address is invalid
#define SEQ_ERR_ACK_MISS  -8 // acknowledge byte is missing

#define SEQ_TYPE_BC        0 // broadcast
#define SEQ_TYPE_MM        1 // master master
#define SEQ_TYPE_MS        2 // master slave

#define SEQ_ACK         0x00 // positive acknowledge
#define SEQ_NAK         0xff // negative acknowledge
#define SEQ_BROAD       0xfe // broadcast destination address
#define SEQ_NN_MAX      0x10 // maximum data bytes

class EbusSequence
{

public:
	EbusSequence();
	explicit EbusSequence(Sequence& seq);

	void parseSequence(Sequence& seq);

	void createMaster(const unsigned char source, const unsigned char target, const std::string& str);
	void createMaster(const unsigned char source, const std::string& str);
	void createMaster(const std::string& str);
	void createMaster(Sequence& seq);

	void createSlave(const std::string& str);
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

	const std::string toString();

	const std::string toStringMaster();
	const std::string toStringMasterCRC();
	const std::string toStringMasterACK() const;
	const std::string toStringMasterError();

	const std::string toStringSlave();
	const std::string toStringSlaveCRC();
	const std::string toStringSlaveACK() const;
	const std::string toStringSlaveError();

	static const std::string errorText(const int error);

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

	static int checkMasterSequence(Sequence& seq);
	static int checkSlaveSequence(Sequence& seq);
};

} // namespace ebusfsm

#endif // EBUSFSM_EBUSSEQUENCE_H

