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

#include "Evaluate.h"
#include "Listen.h"
#include "SendResponse.h"
#include "Common.h"

libebus::Evaluate libebus::Evaluate::m_evaluate;

int libebus::Evaluate::run(EbusFSM* fsm)
{
	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	Action action = fsm->active(eSeq);

	switch (action)
	{
	case Action::noprocess:
		fsm->logWarn(stateMessage(STATE_WRN_NO_PROCESS));
		break;
	case Action::undefined:
		fsm->logWarn(stateMessage(STATE_WRN_NOT_DEF));
		break;
	case Action::ignore:
		fsm->logDebug(stateMessage(STATE_INF_MSG_INGORE));
		break;
	case Action::response:
		eSeq.setSlaveACK(ACK);

		if (eSeq.getSlaveState() == EBUS_OK)
		{
			fsm->logDebug("response: " + eSeq.toStringSlave());
			m_passiveMessage = new EbusMessage(eSeq);
			fsm->changeState(SendResponse::getSendResponse());
			return (DEV_OK);
		}
		else
		{
			fsm->logWarn(stateMessage(STATE_ERR_CREA_MSG));
		}

		break;
	case Action::send:
		if (eSeq.getMasterState() == EBUS_OK)
		{
			fsm->logDebug("enqueue: " + eSeq.toStringMaster());
			fsm->enqueueMessage(new EbusMessage(eSeq, true));
		}
		else
		{
			fsm->logWarn(stateMessage(STATE_ERR_CREA_MSG));
		}

		break;
	default:
		break;
	}

	m_sequence.clear();
	fsm->changeState(Listen::getListen());
	return (DEV_OK);
}

const string libebus::Evaluate::toString() const
{
	return ("Evaluate");
}

