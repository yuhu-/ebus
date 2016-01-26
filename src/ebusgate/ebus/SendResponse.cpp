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

#include "SendResponse.h"
#include "Listen.h"
#include "Logger.h"

extern Logger& L;

SendResponse SendResponse::m_sendResponse;

int SendResponse::run(EbusHandler* h)
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

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	L.log(info, "%s", eSeq.toStringMaster().c_str());

	if (eSeq.getMasterState() == EBUS_OK)
	{
		byte = ACK;
	}
	else
	{
		byte = NAK;
	}

	// send ACK
	result = writeRead(h, byte, 0);
	if (result != DEV_OK) return (result);

	if (eSeq.getMasterState() != EBUS_OK)
	{
		L.log(debug, "%s", errorText(STATE_WRN_RECV_MESS).c_str());
	}
	else if (eSeq.getType() == EBUS_TYPE_MM)
	{
		// doing something meaningful
		L.log(info, "doing something meaningful for %s",
			eSeq.toStringLog().c_str());
	}
	else
	{
		// prepare answer
		L.log(info, "Prepare answer for %s",
			eSeq.toStringLog().c_str());

		if (eSeq.getMaster()[2] == 0x07 && eSeq.getMaster()[3] == 0x04)
		{
			string msg_0704 = "0a7a454741544500010000";
			eSeq.clear();
			eSeq.createSlave(msg_0704);
		}

		for (int retry = 1; retry >= 0; retry--)
		{
			// send Message
			for (size_t i = retry; i < eSeq.getSlave().size(); i++)
			{
				result = writeRead(h, eSeq.getSlave()[i], 0);
				if (result != DEV_OK) return (result);
			}

			// send CRC
			result = writeRead(h, eSeq.getSlaveCRC(), 0);
			if (result != DEV_OK) return (result);

			unsigned char byte;

			// receive ACK
			int result = read(h, byte, 0, h->m_receiveTimeout);
			if (result != DEV_OK) return (result);

			if (byte != ACK && byte != NAK)
			{
				L.log(warn, "%s",
					errorText(STATE_ERR_ACK_WRONG).c_str());
				break;
			}
			else if (byte == ACK)
			{
				break;
			}
			else
			{
				if (retry == 1)
				{
					L.log(debug, "%s",
						errorText(STATE_WRN_ACK_NEG).c_str());
				}
				else
				{
					L.log(warn, "%s",
						errorText(STATE_ERR_ACK_NEG).c_str());
					L.log(info, "%s",
						errorText(STATE_ERR_SEND_FAIL).c_str());
				}
			}
		}
	}

	reset(h);
	h->changeState(Listen::getInstance());

	return (DEV_OK);
}

SendResponse::SendResponse()
{
}

const char* SendResponse::toString() const
{
	return ("SendResponse");
}

