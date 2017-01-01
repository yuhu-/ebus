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
#include "Logger.h"

SendResponse SendResponse::m_sendResponse;

int SendResponse::run(EbusFSM* fsm)
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
			LIBLOGGER_INFO("%s", errorText(STATE_ERR_ACK_WRONG).c_str())
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
				LIBLOGGER_INFO("%s", errorText(STATE_WRN_ACK_NEG).c_str())
			}
			else
			{
				LIBLOGGER_INFO("%s", errorText(STATE_ERR_ACK_NEG).c_str())
				LIBLOGGER_INFO("%s", errorText(STATE_ERR_SEND_FAIL).c_str())
			}
		}
	}

	eSeq.setMasterACK(byte);
	LIBLOGGER_INFO("%s", eSeq.toStringLog().c_str())

	if (fsm->m_process != nullptr) fsm->m_process->passive(eSeq);

	reset(fsm);
	fsm->changeState(Listen::getListen());

	return (DEV_OK);
}

const string SendResponse::toString() const
{
	return ("SendResponse");
}

