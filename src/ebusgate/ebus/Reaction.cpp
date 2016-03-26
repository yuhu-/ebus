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

#include "Listen.h"
#include "SendResponse.h"
#include "Common.h"
#include "Logger.h"

#include <algorithm>
#include "Reaction.h"

using std::copy_n;
using std::back_inserter;

Reaction Reaction::m_reaction;

int Reaction::run(EbusHandler* h)
{
	Logger logger = Logger("Reaction::run");

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	int action = findAction(eSeq);
	unsigned char target = 0;

	logger.debug("handle action type %d", action);

	switch (action)
	{
	case at_notDefined:
		logger.warn("%s", errorText(STATE_WRN_NOT_DEF).c_str());
		break;
	case at_doNothing:
	case at_response:
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

	if (action == at_response)
	{
		eSeq.setSlaveACK(ACK);

		if (createResponse(eSeq) == true)
		{
			logger.debug("response: %s", eSeq.toStringSlave().c_str());
			m_passiveMessage = new EbusMessage(eSeq);
			h->changeState(SendResponse::getSendResponse());
			return (DEV_OK);
		}
		else
		{
			logger.warn("%s", STATE_ERR_CREA_MSG);
		}
	}
	else if (action > at_response)
	{
		if (createMessage(h->m_address, target, eSeq) == true)
		{
			logger.debug("enqueue: %s", eSeq.toStringMaster().c_str());
			h->enqueue(new EbusMessage(eSeq));
		}
		else
		{
			logger.warn("%s", STATE_ERR_CREA_MSG);
		}
	}

	m_sequence.clear();
	h->changeState(Listen::getListen());
	return (DEV_OK);
}

Reaction::Reaction()
{
}

const string Reaction::toString() const
{
	return ("Reaction");
}

int Reaction::findAction(const EbusSequence& eSeq)
{
	for (Action action : m_action)
		if (action.match(eSeq.getMaster()) == true) return (action.getType());

	return (at_notDefined);
}

bool Reaction::createResponse(EbusSequence& eSeq)
{
	for (Action action : m_action)
		if (action.match(eSeq.getMaster()) == true)
		{
			Sequence seq(action.getMessage());
			eSeq.createSlave(seq);
			if (eSeq.getSlaveState() == EBUS_OK) return (true);
			break;
		}

	return (false);
}

bool Reaction::createMessage(const unsigned char source, const unsigned char target, EbusSequence& eSeq)
{
	for (Action action : m_action)
		if (action.match(eSeq.getMaster()) == true)
		{
			eSeq.clear();
			eSeq.createMaster(source, target, action.getMessage());
			if (eSeq.getSlaveState() == EBUS_OK) return (true);
			break;
		}

	return (false);
}

