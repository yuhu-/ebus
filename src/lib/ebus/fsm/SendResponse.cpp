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

#include "SendResponse.h"
#include "Listen.h"

libebus::SendResponse libebus::SendResponse::m_sendResponse;

int libebus::SendResponse::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_passiveMessage->getEbusSequence();
	int result;
	unsigned char byte;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < eSeq.getSlave().size(); i++)
		{
			result = writeRead(fsm, eSeq.getSlave()[i], 0);
			if (result != DEV_OK) return (result);
		}

		// send CRC
		result = writeRead(fsm, eSeq.getSlaveCRC(), 0);
		if (result != DEV_OK) return (result);

		// receive ACK
		int result = read(fsm, byte, 0, fsm->m_receiveTimeout);
		if (result != DEV_OK) return (result);

		if (byte != ACK && byte != NAK)
		{
			fsm->logInfo(stateMessage(STATE_ERR_ACK_WRONG));
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
				fsm->logInfo(stateMessage(STATE_WRN_ACK_NEG));
			}
			else
			{
				fsm->logInfo(stateMessage(STATE_ERR_ACK_NEG));
				fsm->logInfo(stateMessage(STATE_ERR_SEND_FAIL));
			}
		}
	}

	eSeq.setMasterACK(byte);

	fsm->logInfo(eSeq.toStringLog());
	fsm->passiveMessage(eSeq);

	reset(fsm);
	fsm->changeState(Listen::getListen());

	return (DEV_OK);
}

const string libebus::SendResponse::toString() const
{
	return ("SendResponse");
}

