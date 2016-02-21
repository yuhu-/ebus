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

#include "Action.h"
#include "Listen.h"
#include "SendResponse.h"
#include "Common.h"
#include "Logger.h"

#include <algorithm>

using std::copy_n;
using std::back_inserter;

Action Action::m_action;

enum ActionType
{
	at_notDefined,	// not defined
	at_doNothing,	// do nothing
	at_response,	// prepare slave part and send response
	at_send_BC,	// create and send broadcast message
	at_send_MM,	// create and send master master message
	at_send_MS	// create and send master slave message
};

map<vector<unsigned char>, int> ActionTypes =
{
{
{ 0x07, 0x00 }, at_doNothing },
{
{ 0x07, 0x04 }, at_response },
{
{ 0x07, 0xfe }, at_send_BC },
{
{ 0xb5, 0x05 }, at_doNothing },
{
{ 0xb5, 0x16 }, at_doNothing } };

map<vector<unsigned char>, string> ActionMessages =
{
{
{ 0x07, 0x04 }, "0a7a454741544501010101" },
{
{ 0x07, 0xfe }, "07ff00" } };

int Action::run(EbusHandler* h)
{
	Logger& L = Logger::getLogger("Action::run");

	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	int action = findAction(eSeq);
	unsigned char target = 0;

	L.log(debug, "handle action type %d", action);

	switch (action)
	{
	case at_notDefined:
		L.log(warn, "%s", errorText(STATE_WRN_NOT_DEF).c_str());
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
			L.log(debug, "response: %s", eSeq.toStringSlave().c_str());
			m_passiveMessage = new EbusMessage(eSeq);
			h->changeState(SendResponse::getSendResponse());
			return (DEV_OK);
		}
		else
		{
			L.log(warn, "%s", STATE_ERR_CREA_MSG);
		}
	}
	else if (action > at_response)
	{
		if (createMessage(h->m_address, target, eSeq) == true)
		{
			L.log(debug, "enqueue: %s", eSeq.toStringMaster().c_str());
			h->addMessage(new EbusMessage(eSeq));
		}
		else
		{
			L.log(warn, "%s", STATE_ERR_CREA_MSG);
		}
	}

	m_sequence.clear();
	h->changeState(Listen::getListen());
	return (DEV_OK);
}

Action::Action()
{
}

const string Action::toString() const
{
	return ("Action");
}

// TODO implement search for variable key length
int Action::findAction(const EbusSequence& eSeq)
{
	vector<unsigned char> key;

	copy_n(eSeq.getMaster().getSequence().begin() + 2, 2, back_inserter(key));

	map<vector<unsigned char>, int>::iterator it = ActionTypes.find(key);

	if (it != ActionTypes.end()) return (it->second);

	return (at_notDefined);
}

// TODO implement search for variable key length
bool Action::createResponse(EbusSequence& eSeq)
{
	vector<unsigned char> key;

	copy_n(eSeq.getMaster().getSequence().begin() + 2, 2, back_inserter(key));

	map<vector<unsigned char>, string>::iterator it = ActionMessages.find(key);

	if (it != ActionMessages.end())
	{
		eSeq.createSlave(it->second);
		if (eSeq.getSlaveState() == EBUS_OK) return (true);
	}

	return (false);
}

// TODO implement search for variable key length
bool Action::createMessage(const unsigned char source, const unsigned char target, EbusSequence& eSeq)
{
	vector<unsigned char> key;

	copy_n(eSeq.getMaster().getSequence().begin() + 2, 2, back_inserter(key));

	map<vector<unsigned char>, string>::iterator it = ActionMessages.find(key);

	if (it != ActionMessages.end())
	{
		eSeq.clear();
		eSeq.createMaster(source, target, it->second);
		if (eSeq.getMasterState() == EBUS_OK) return (true);
	}

	return (false);
}

