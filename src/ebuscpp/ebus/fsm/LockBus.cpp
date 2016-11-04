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

#include "../../../ebuscpp/ebus/fsm/LockBus.h"

#include "Logger.h"

#include <unistd.h>
#include "../../../ebuscpp/ebus/fsm/Listen.h"
#include "../../../ebuscpp/ebus/fsm/SendMessage.h"

LockBus LockBus::m_lockBus;

int LockBus::run(EbusFSM* fsm)
{
	Logger logger = Logger("LockBus::run");

	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	if (eSeq.getMasterState() != EBUS_OK)
	{
		logger.debug("%s", eSeq.toStringMaster().c_str());
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
		logger.debug("%s", errorText(STATE_WRN_ARB_LOST).c_str());

		if (m_lockRetries < fsm->m_lockRetries)
		{
			m_lockRetries++;

			if ((byte & 0x0f) != (eSeq.getMaster()[0] & 0x0f))
			{
				m_lockCounter = fsm->m_lockCounter;
				logger.debug("%s", errorText(STATE_WRN_PRI_LOST).c_str());
			}
			else
			{
				m_lockCounter = 1;
				logger.debug("%s", errorText(STATE_INF_PRI_FIT).c_str());
			}
		}
		else
		{
			logger.warn("%s", errorText(STATE_ERR_LOCK_FAIL).c_str());
			m_activeMessage->setResult(errorText(STATE_ERR_LOCK_FAIL));

			reset(fsm);
		}

		fsm->changeState(Listen::getListen());
	}
	else
	{
		logger.debug("ebus locked");
		fsm->changeState(SendMessage::getSendMessage());
	}

	return (result);
}

const string LockBus::toString() const
{
	return ("LockBus");
}
