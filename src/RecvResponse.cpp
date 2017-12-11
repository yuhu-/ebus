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

#include <RecvResponse.h>
#include <FreeBus.h>
#include <Listen.h>

ebusfsm::RecvResponse ebusfsm::RecvResponse::m_recvResponse;

int ebusfsm::RecvResponse::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	unsigned char byte;
	Sequence seq;
	int result;

	for (int retry = 1; retry >= 0; retry--)
	{
		// receive NN
		result = read(fsm, byte, 1, 0);
		if (result != DEV_OK) return (result);

		// maximum data bytes
		if (byte > SEQ_NN_MAX)
		{
			fsm->logWarn(stateMessage(fsm, STATE_ERR_NN_WRONG));
			m_activeMessage->setState(FSM_ERR_TRANSMIT);

			reset(fsm);
			fsm->changeState(Listen::getListen());
			return (DEV_OK);
		}

		seq.push_back(byte);

		// +1 for CRC
		size_t bytes = byte + 1;

		for (size_t i = 0; i < bytes; i++)
		{
			result = read(fsm, byte, 1, 0);
			if (result != DEV_OK) return (result);

			seq.push_back(byte);

			if (byte == SYN || byte == EXT) bytes++;
		}

		// create slave data
		eSeq.createSlave(seq);

		if (eSeq.getSlaveState() == SEQ_OK)
			byte = SEQ_ACK;
		else
			byte = SEQ_NAK;

		// send ACK
		result = writeRead(fsm, byte, 0, 0);
		if (result != DEV_OK) return (result);

		eSeq.setMasterACK(byte);

		if (eSeq.getSlaveState() == SEQ_OK)
		{
			fsm->logDebug(m_sequence.toString());
			fsm->logInfo(eSeqMessage(fsm, eSeq) + " transmitted");
			break;
		}

		if (retry == 1)
		{
			seq.clear();
			fsm->logDebug(stateMessage(fsm, STATE_WRN_RECV_RESP));
		}
		else
		{
			fsm->logWarn(stateMessage(fsm, STATE_ERR_RECV_RESP));
			m_activeMessage->setState(FSM_ERR_TRANSMIT);
		}
	}

	fsm->changeState(FreeBus::getFreeBus());

	return (result);
}

const std::string ebusfsm::RecvResponse::toString() const
{
	return ("RecvResponse");
}
