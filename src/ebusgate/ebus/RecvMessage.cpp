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

#include "RecvMessage.h"
#include "Listen.h"
#include "Action.h"
#include "Logger.h"

extern Logger& L;

RecvMessage RecvMessage::m_recvMessage;

int RecvMessage::run(EbusHandler* h)
{
	int result;
	unsigned char byte;

	// receive Header PBSBNN
	for (int i = 0; i < 3; i++)
	{
		unsigned char byte = 0;

		result = read(h, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);
	}

	// check against max. possible size
	if (m_sequence[4] > 16)
	{
		L.log(warn, "%s", errorText(STATE_ERR_NN_WRONG).c_str());
		reset(h);
		h->changeState(Listen::getInstance());
		return (DEV_OK);
	}

	// receive Data Dx
	for (int i = 0; i < m_sequence[4]; i++)
	{
		unsigned char byte = 0;

		result = read(h, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);
	}

	// 1 for CRC
	int bytes = 1;

	// receive CRC
	for (int i = 0; i < bytes; i++)
	{
		result = read(h, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);

		if (byte == SYN || byte == EXT) bytes++;
	}

	L.log(debug, "%s", m_sequence.toString().c_str());

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	if (m_sequence[1] != BROADCAST)
	{
		if (eSeq.getMasterState() == EBUS_OK)
		{
			byte = ACK;
		}
		else
		{
			byte = NAK;
			L.log(info, "%s", errorText(STATE_WRN_RECV_MSG).c_str());
		}

		// send ACK
		result = writeRead(h, byte, 0);
		if (result != DEV_OK) return (result);
	}

	if (eSeq.getMasterState() == EBUS_OK)
	{
		if (eSeq.getType() != EBUS_TYPE_MS)
		{
			if (eSeq.getType() == EBUS_TYPE_MM) eSeq.setSlaveACK(byte);

			L.log(info, "%s", eSeq.toStringLog().c_str());

			if (h->m_store == true) h->storeMessage(eSeq);
		}

		if (h->m_active == true)
		{
			h->changeState(Action::getInstance());
			return (DEV_OK);
		}
	}

	m_sequence.clear();
	h->changeState(Listen::getInstance());
	return (DEV_OK);
}

RecvMessage::RecvMessage()
{
}

const char* RecvMessage::toString() const
{
	return ("RecvMessage");
}

