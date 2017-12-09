/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
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

#include <SendResponse.h>
#include <Listen.h>

ebusfsm::SendResponse ebusfsm::SendResponse::m_sendResponse;

int ebusfsm::SendResponse::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_passiveMessage->getEbusSequence();
	int result;
	unsigned char byte;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < eSeq.getSlave().size(); i++)
		{
			result = writeRead(fsm, eSeq.getSlave()[i], 0, 0);
			if (result != DEV_OK) return (result);
		}

		// send CRC
		result = writeRead(fsm, eSeq.getSlaveCRC(), 0, 0);
		if (result != DEV_OK) return (result);

		// receive ACK
		int result = read(fsm, byte, 0, fsm->m_receiveTimeout);
		if (result != DEV_OK) return (result);

		if (byte != SEQ_ACK && byte != SEQ_NAK)
		{
			fsm->logInfo(stateMessage(fsm, STATE_ERR_ACK_WRONG));
			break;
		}
		else if (byte == SEQ_ACK)
		{
			break;
		}
		else
		{
			if (retry == 1)
			{
				fsm->logInfo(stateMessage(fsm, STATE_WRN_ACK_NEG));
			}
			else
			{
				fsm->logInfo(stateMessage(fsm, STATE_ERR_ACK_NEG));
				fsm->logInfo(stateMessage(fsm, STATE_ERR_RESP_SEND));
			}
		}
	}

	eSeq.setMasterACK(byte);

	fsm->logInfo(eSeqMessage(fsm, eSeq));
	fsm->publish(eSeq);

	reset(fsm);
	fsm->changeState(Listen::getListen());

	return (DEV_OK);
}

const std::string ebusfsm::SendResponse::toString() const
{
	return ("SendResponse");
}

