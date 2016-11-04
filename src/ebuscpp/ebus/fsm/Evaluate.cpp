/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#include "Evaluate.h"
#include "Listen.h"
#include "SendResponse.h"
#include "Common.h"
#include "Logger.h"

Evaluate Evaluate::m_evaluate;

int Evaluate::run(EbusFSM* fsm)
{
	Logger logger = Logger("Evaluate::run");

	if (fsm->m_process != nullptr)
	{
		EbusSequence eSeq;
		eSeq.createMaster(m_sequence);

		ActionType type = fsm->m_process->active(eSeq);

		switch (type)
		{
		case at_undefined:
			logger.warn("%s", errorText(STATE_WRN_NOT_DEF).c_str());
			break;
		case at_ignore:
			logger.debug("ignore");
			break;
		case at_response:
			eSeq.setSlaveACK(ACK);

			if (eSeq.getSlaveState() == EBUS_OK)
			{
				logger.debug("response: %s", eSeq.toStringSlave().c_str());
				m_passiveMessage = new EbusMessage(eSeq);
				fsm->changeState(SendResponse::getSendResponse());
				return (DEV_OK);
			}
			else
			{
				logger.warn("%s", errorText(STATE_ERR_CREA_MSG).c_str());
			}

			break;
		case at_send:
			if (eSeq.getMasterState() == EBUS_OK)
			{
				logger.debug("enqueue: %s", eSeq.toStringMaster().c_str());
				fsm->enqueue(new EbusMessage(eSeq, true));
			}
			else
			{
				logger.warn("%s", errorText(STATE_ERR_CREA_MSG).c_str());
			}

			break;
		default:
			break;
		}
	}
	else
	{
		logger.warn("process not implemented");
	}

	m_sequence.clear();
	fsm->changeState(Listen::getListen());
	return (DEV_OK);
}

const string Evaluate::toString() const
{
	return ("Evaluate");
}

