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

#include "SendMessage.h"
#include "FreeBus.h"
#include "Listen.h"
#include "RecvResponse.h"

libebus::SendMessage libebus::SendMessage::m_sendMessage;

int libebus::SendMessage::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	int result;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < eSeq.getMaster().size(); i++)
		{
			result = writeRead(fsm, eSeq.getMaster()[i], 0);
			if (result != DEV_OK) return (result);
		}

		// send CRC
		result = writeRead(fsm, eSeq.getMasterCRC(), 0);
		if (result != DEV_OK) return (result);

		// Broadcast ends here
		if (eSeq.getType() == SEQ_TYPE_BC)
		{
			fsm->logInfo(eSeqMessage(fsm, eSeq) + " transmitted");
			fsm->changeState(FreeBus::getFreeBus());
			break;
		}

		unsigned char byte;

		// receive ACK
		int result = read(fsm, byte, 0, fsm->m_receiveTimeout);
		if (result != DEV_OK) return (result);

		eSeq.setSlaveACK(byte);

		if (byte != SEQ_ACK && byte != SEQ_NAK)
		{
			fsm->logWarn(stateMessage(fsm, STATE_ERR_ACK_WRONG));
			m_activeMessage->setState(FSM_ERR_TRANSMIT);

			fsm->changeState(FreeBus::getFreeBus());
			break;
		}
		else if (byte == SEQ_ACK)
		{
			// Master Master ends here
			if (eSeq.getType() == SEQ_TYPE_MM)
			{
				fsm->logInfo(eSeqMessage(fsm, eSeq) + " transmitted");
				fsm->changeState(FreeBus::getFreeBus());
			}
			else
			{
				fsm->changeState(RecvResponse::getRecvResponse());
			}

			break;
		}
		else
		{
			if (retry == 1)
			{
				fsm->logDebug(stateMessage(fsm, STATE_WRN_ACK_NEG));
			}
			else
			{
				fsm->logWarn(stateMessage(fsm, STATE_ERR_ACK_NEG));
				m_activeMessage->setState(FSM_ERR_TRANSMIT);

				fsm->changeState(FreeBus::getFreeBus());
			}
		}
	}

	return (result);
}

const string libebus::SendMessage::toString() const
{
	return ("SendMessage");
}
