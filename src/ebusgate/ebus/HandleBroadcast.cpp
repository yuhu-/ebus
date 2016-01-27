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

#include "HandleBroadcast.h"
#include "Listen.h"
#include "Logger.h"
#include "Common.h"

#include <algorithm>

using std::copy_n;
using std::back_inserter;

extern Logger& L;

enum ActionType
{
	at_none, at_log, at_response_BC, at_response_MM, at_response_MS
};

HandleBroadcast HandleBroadcast::m_handleBroadcast;

map<vector<unsigned char>, int> BroadcastActions =
{
{
{ 0x07, 0x00 }, at_log },
{
{ 0x07, 0xfe }, at_response_BC },
{
{ 0xb5, 0x05 }, at_log } };

map<vector<unsigned char>, string> BroadcastMessages =
{
{
{ 0x07, 0xfe }, "07ff00" } };

int HandleBroadcast::run(EbusHandler* h)
{
	EbusSequence eSeq;
	eSeq.createMaster(m_sequence);

	int action = needAction(eSeq);
	unsigned char target = 0;

	if (action != at_none)
		L.log(info, "handle action %d for %s", needAction(eSeq),
			eSeq.toStringLog().c_str());

	switch (action)
	{
	case at_response_BC:
		target = BROADCAST;
		break;
	case at_response_MM:
		target = eSeq.getMasterSource();
		break;
	case at_response_MS:
		target = slaveAddress(eSeq.getMasterSource());
		break;
	case at_none:
	case at_log:
	default:
		break;
	}

	if (action > at_log
		&& createMessage(h->m_address, target, eSeq) == true)
	{
		L.log(info, "ready for send - %s",
			eSeq.toStringMaster().c_str());

		// TODO implement send
	}
	else
	{
		L.log(info, "error during build - %s",
			eSeq.toStringMaster().c_str());
	}

	h->changeState(Listen::getInstance());

	m_sequence.clear();
	return (DEV_OK);
}

HandleBroadcast::HandleBroadcast()
{
}

const char* HandleBroadcast::toString() const
{
	return ("HandleBroadcast");
}

int HandleBroadcast::needAction(const EbusSequence& eSeq)
{
	vector<unsigned char> key;

	copy_n(eSeq.getMaster().getSequence().begin() + 2, 2,
		back_inserter(key));

	map<vector<unsigned char>, int>::iterator it = BroadcastActions.find(
		key);

	if (it != BroadcastActions.end()) return (it->second);

	return (at_none);
}

bool HandleBroadcast::createMessage(const unsigned char source,
	const unsigned char target, EbusSequence& eSeq)
{
	vector<unsigned char> key;

	copy_n(eSeq.getMaster().getSequence().begin() + 2, 2,
		back_inserter(key));

	map<vector<unsigned char>, string>::iterator it =
		BroadcastMessages.find(key);

	if (it != BroadcastMessages.end())
	{
		eSeq.clear();
		eSeq.createMaster(source, target, it->second);
		if (eSeq.getMasterState() == EBUS_OK) return (true);
	}

	return (false);
}

