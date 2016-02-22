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

SendResponse SendResponse::m_sendResponse;

int SendResponse::run(EbusHandler* h)
{
	Logger L = Logger("SendResponse::run");

	EbusSequence& eSeq = m_passiveMessage->getEbusSequence();
	int result;
	unsigned char byte;

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

		// receive ACK
		int result = read(h, byte, 0, h->m_receiveTimeout);
		if (result != DEV_OK) return (result);

		if (byte != ACK && byte != NAK)
		{
			L.log(info, "%s", errorText(STATE_ERR_ACK_WRONG).c_str());
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
				L.log(info, "%s", errorText(STATE_WRN_ACK_NEG).c_str());
			}
			else
			{
				L.log(info, "%s", errorText(STATE_ERR_ACK_NEG).c_str());
				L.log(info, "%s", errorText(STATE_ERR_SEND_FAIL).c_str());
			}
		}
	}

	eSeq.setMasterACK(byte);
	L.log(info, "%s", eSeq.toStringLog().c_str());

	reset(h);
	h->changeState(Listen::getListen());

	return (DEV_OK);
}

SendResponse::SendResponse()
{
}

const string SendResponse::toString() const
{
	return ("SendResponse");
}

