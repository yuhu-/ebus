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

LockBus LockBus::m_lockBus;

int LockBus::run(EbusFSM* fsm)
{
	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	if (eSeq.getMasterState() != EBUS_OK)
	{
		fsm->m_logger->debug(eSeq.toStringMaster());
		m_activeMessage->setResult(eSeq.toStringMaster());

		reset(fsm);
		fsm->changeState(Listen::getListen());
		return (DEV_OK);
	}

	unsigned char byte = eSeq.getMaster()[0];

	int result = write(fsm, byte);
	if (result != DEV_OK) return (result);

	usleep(fsm->m_arbitrationTime);

	byte = 0;

	result = read(fsm, byte, 0, fsm->m_receiveTimeout);
	if (result != DEV_OK) return (result);

	if (byte != eSeq.getMaster()[0])
	{
		fsm->m_logger->debug(stateMessage(STATE_WRN_ARB_LOST));

		if (m_lockRetries < fsm->m_lockRetries)
		{
			m_lockRetries++;

			if ((byte & 0x0f) != (eSeq.getMaster()[0] & 0x0f))
			{
				m_lockCounter = fsm->m_lockCounter;
				fsm->m_logger->debug(stateMessage(STATE_WRN_PRI_LOST));
			}
			else
			{
				m_lockCounter = 1;
				fsm->m_logger->debug(stateMessage(STATE_WRN_PRI_FIT));
			}
		}
		else
		{
			fsm->m_logger->warn(stateMessage(STATE_ERR_LOCK_FAIL));
			m_activeMessage->setResult(stateMessage(STATE_ERR_LOCK_FAIL));

			reset(fsm);
		}

		fsm->changeState(Listen::getListen());
	}
	else
	{
		fsm->m_logger->debug(stateMessage(STATE_INF_EBUS_LOCK));
		fsm->changeState(SendMessage::getSendMessage());
	}

	return (result);
}

const string LockBus::toString() const
{
	return ("LockBus");
}
