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

static std::byte seq_ack;    // 0x00 positive acknowledge
static std::byte seq_nak;    // 0xff negative acknowledge
static std::byte seq_broad;  // 0xfe broadcast destination address

static int seq_max_bytes;    // 16 maximum data bytes

class EbusSequence
{

public:
	EbusSequence();
	explicit EbusSequence(Sequence& seq);

	void parseSequence(Sequence& seq);

	void createMaster(const std::byte source, const std::byte target, const std::string& str);
	void createMaster(const std::byte source, const std::string& str);
	void createMaster(const std::string& str);
	void createMaster(Sequence& seq);

	void createSlave(const std::string& str);
	void createSlave(Sequence& seq);

	void clear();

	std::byte getMasterQQ() const;
	std::byte getMasterZZ() const;

	Sequence getMaster() const;
	size_t getMasterNN() const;
	std::byte getMasterCRC() const;
	int getMasterState() const;

	void setSlaveACK(const std::byte byte);

	Sequence getSlave() const;
	size_t getSlaveNN() const;
	std::byte getSlaveCRC() const;
	int getSlaveState() const;

	void setMasterACK(const std::byte byte);

	void setType(const std::byte byte);
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

	std::byte m_masterQQ = seq_zero;
	std::byte m_masterZZ = seq_zero;

	Sequence m_master;
	size_t m_masterNN = 0;
	std::byte m_masterCRC = seq_zero;
	int m_masterState = SEQ_EMPTY;

	std::byte m_slaveACK = seq_zero;

	Sequence m_slave;
	size_t m_slaveNN = 0;
	std::byte m_slaveCRC = seq_zero;
	int m_slaveState = SEQ_EMPTY;

	std::byte m_masterACK = seq_zero;

	static int checkMasterSequence(Sequence& seq);
	static int checkSlaveSequence(Sequence& seq);
};

} // namespace ebusfsm

#endif // EBUSFSM_EBUSSEQUENCE_H

