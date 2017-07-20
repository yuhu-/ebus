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

#include "LockBus.h"
#include "Listen.h"
#include "SendMessage.h"

#include <unistd.h>

libebus::LockBus libebus::LockBus::m_lockBus;

int libebus::LockBus::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	unsigned char byte = eSeq.getMasterQQ();

	int result = write(fsm, byte);
	if (result != DEV_OK) return (result);

	usleep(fsm->m_arbitrationTime);

	byte = 0;

	result = read(fsm, byte, 0, fsm->m_receiveTimeout);
	if (result != DEV_OK) return (result);

	if (byte != eSeq.getMasterQQ())
	{
		fsm->logDebug(stateMessage(fsm, STATE_WRN_ARB_LOST));

		if (m_lockRetries < fsm->m_lockRetries)
		{
			m_lockRetries++;

			if ((byte & 0x0f) != (eSeq.getMasterQQ() & 0x0f))
			{
				m_lockCounter = fsm->m_lockCounter;
				fsm->logDebug(stateMessage(fsm, STATE_WRN_PRI_LOST));
			}
			else
			{
				m_lockCounter = 1;
				fsm->logDebug(stateMessage(fsm, STATE_WRN_PRI_FIT));
			}
		}
		else
		{
			fsm->logWarn(stateMessage(fsm, STATE_ERR_LOCK_FAIL));
			m_activeMessage->setState(FSM_ERR_TRANSMIT);

			reset(fsm);
		}

		fsm->changeState(Listen::getListen());
	}
	else
	{
		fsm->logDebug(stateMessage(fsm, STATE_INF_EBUS_LOCK));
		fsm->changeState(SendMessage::getSendMessage());
	}

	return (result);
}

const string libebus::LockBus::toString() const
{
	return ("LockBus");
}
