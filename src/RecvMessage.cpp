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

#include <RecvMessage.h>
#include <EvalMessage.h>
#include <Listen.h>

ebusfsm::RecvMessage ebusfsm::RecvMessage::m_recvMessage;

int ebusfsm::RecvMessage::run(EbusFSM* fsm)
{
	int result;
	unsigned char byte;

	// receive Header PBSBNN
	for (int i = 0; i < 3; i++)
	{
		byte = 0;

		result = read(fsm, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);
	}

	// maximum data bytes
	if (m_sequence[4] > SEQ_NN_MAX)
	{
		fsm->logWarn(stateMessage(fsm, STATE_ERR_NN_WRONG));
		m_activeMessage->setState(FSM_ERR_TRANSMIT);

		reset(fsm);
		fsm->changeState(Listen::getListen());
		return (DEV_OK);
	}

	// bytes to receive
	int bytes = m_sequence[4];

	// receive Data Dx
	for (int i = 0; i < bytes; i++)
	{
		byte = 0;

		result = read(fsm, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);

		if (byte == EXT) bytes++;
	}

	// 1 for CRC
	bytes = 1;

	// receive CRC
	for (int i = 0; i < bytes; i++)
	{
		result = read(fsm, byte, 1, 0);
		if (result != DEV_OK) return (result);

		m_sequence.push_back(byte);

		if (byte == EXT) bytes++;
	}

	fsm->logDebug(m_sequence.toString());

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	if (m_sequence[1] != SEQ_BROAD)
	{
		if (eSeq.getMasterState() == SEQ_OK)
		{
			byte = SEQ_ACK;
		}
		else
		{
			byte = SEQ_NAK;
			fsm->logInfo(stateMessage(fsm, STATE_WRN_RECV_MSG));
		}

		// send ACK
		result = writeRead(fsm, byte, 0, 0);
		if (result != DEV_OK) return (result);

		eSeq.setSlaveACK(byte);
	}

	if (eSeq.getMasterState() == SEQ_OK)
	{
		if (eSeq.getType() != SEQ_TYPE_MS)
		{
			fsm->logInfo(eSeqMessage(fsm, eSeq));
			fsm->publish(eSeq);
		}

		fsm->changeState(EvalMessage::getEvalMessage());
		return (DEV_OK);
	}

	m_sequence.clear();
	fsm->changeState(Listen::getListen());
	return (DEV_OK);
}

const std::string ebusfsm::RecvMessage::toString() const
{
	return ("RecvMessage");
}

