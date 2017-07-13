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

#include "Listen.h"
#include "LockBus.h"
#include "RecvMessage.h"
#include "EbusCommon.h"

#include <string>

using std::to_string;

libebus::Listen libebus::Listen::m_listen;

int libebus::Listen::run(EbusFSM* fsm)
{
	unsigned char byte = 0;

	int result = read(fsm, byte, 1, 0);
	if (result != DEV_OK) return (result);

	if (byte == SYN)
	{
		if (m_lockCounter != 0)
		{
			m_lockCounter--;
			fsm->logDebug("lockCounter: " + to_string(m_lockCounter));
		}

		// decode EbusSequence
		if (m_sequence.size() != 0)
		{
			fsm->logDebug(m_sequence.toString());

			EbusSequence eSeq(m_sequence);
			fsm->logInfo(eSeq.toStringLog());

			if (eSeq.isValid() == true) fsm->publishMessage(eSeq);

			if (m_sequence.size() == 1 && m_lockCounter < 2) m_lockCounter = 2;

			eSeq.clear();
			m_sequence.clear();
		}

		// check for new EbusMessage
		if (m_activeMessage == nullptr && fsm->m_ebusMsgQueue.size() > 0) m_activeMessage = fsm->m_ebusMsgQueue.dequeue();

		// handle EbusMessage
		if (m_activeMessage != nullptr && m_lockCounter == 0) fsm->changeState(LockBus::getLockBus());
	}
	else
	{
		m_sequence.push_back(byte);

		// handle broadcast and at me addressed messages
		if (m_sequence.size() == 2
			&& (m_sequence[1] == BROADCAST || m_sequence[1] == fsm->m_address
				|| m_sequence[1] == fsm->m_slaveAddress))
			fsm->changeState(RecvMessage::getRecvMessage());

	}

	return (result);
}

const string libebus::Listen::toString() const
{
	return ("Listen");
}

