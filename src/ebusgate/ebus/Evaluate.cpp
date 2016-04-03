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

#include "Evaluate.h"
#include "Listen.h"
#include "SendResponse.h"
#include "Common.h"
#include "Logger.h"

Evaluate Evaluate::m_evaluate;

int Evaluate::run(EbusHandler* h)
{
	Logger logger = Logger("Evaluate::run");

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	ActionType action = h->getType(eSeq);
	unsigned char target = 0;

	logger.debug("action type %d", action);

	switch (action)
	{
	case at_undefined:
		logger.warn("%s", errorText(STATE_WRN_NOT_DEF).c_str());
		break;
	case at_ignore:
		logger.debug("ignore");
		break;
	case at_response:
		eSeq.setSlaveACK(ACK);

		if (h->createResponse(eSeq) == true)
		{
			logger.debug("response: %s", eSeq.toStringSlave().c_str());
			m_passiveMessage = new EbusMessage(eSeq);
			h->changeState(SendResponse::getSendResponse());
			return (DEV_OK);
		}
		else
		{
			logger.warn("%s", errorText(STATE_ERR_CREA_MSG).c_str());
		}

		break;
	case at_send_BC:
		target = BROADCAST;
		break;
	case at_send_MM:
		target = eSeq.getMasterQQ();
		break;
	case at_send_MS:
		target = slaveAddress(eSeq.getMasterQQ());
		break;
	default:
		break;
	}

	if (action > at_response)
	{
		if (h->createMessage(target, eSeq) == true)
		{
			logger.debug("enqueue: %s", eSeq.toStringMaster().c_str());
			h->enqueue(new EbusMessage(eSeq, true));
		}
		else
		{
			logger.warn("%s", errorText(STATE_ERR_CREA_MSG).c_str());
		}
	}

	m_sequence.clear();
	h->changeState(Listen::getListen());
	return (DEV_OK);
}

Evaluate::Evaluate()
{
}

const string Evaluate::toString() const
{
	return ("Evaluate");
}

