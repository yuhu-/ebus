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

#include "EvalMessage.h"
#include "Listen.h"
#include "SendResponse.h"
#include "EbusCommon.h"

libebus::EvalMessage libebus::EvalMessage::m_evalMessage;

int libebus::EvalMessage::run(EbusFSM* fsm)
{
	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	Reaction reaction = fsm->identify(eSeq);

	switch (reaction)
	{
	case Reaction::nofunction:
		fsm->logWarn(stateMessage(fsm, STATE_WRN_NO_FUNC));
		break;
	case Reaction::undefined:
		fsm->logWarn(stateMessage(fsm, STATE_WRN_NOT_DEF));
		break;
	case Reaction::ignore:
		fsm->logInfo(stateMessage(fsm, STATE_INF_MSG_INGORE));
		break;
	case Reaction::response:
		if (eSeq.getType() == SEQ_TYPE_MS)
		{
			if (eSeq.getSlaveState() == SEQ_OK)
			{
				fsm->logInfo("response: " + eSeq.toStringSlave());
				m_passiveMessage = new Message(eSeq);
				fsm->changeState(SendResponse::getSendResponse());
				return (DEV_OK);
			}
			else
			{
				fsm->logWarn(stateMessage(fsm, STATE_ERR_RESP_CREA));
			}
		}
		else
		{
			fsm->logWarn(stateMessage(fsm, STATE_ERR_BAD_TYPE));
		}

		break;
	default:
		break;
	}

	m_sequence.clear();
	fsm->changeState(Listen::getListen());
	return (DEV_OK);
}

const string libebus::EvalMessage::toString() const
{
	return ("EvalMessage");
}

