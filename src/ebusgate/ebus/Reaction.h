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

#ifndef EBUS_REACTION_H
#define EBUS_REACTION_H

#include "State.h"
#include "Action.h"

class Reaction : public State
{

public:
	static Reaction* getReaction()
	{
		return (&m_reaction);
	}

	int run(EbusHandler* h);
	const string toString() const;

private:
	Reaction();
	static Reaction m_reaction;

	vector<Action> m_action =
	{
	{ Action("0700", at_doNothing, "") },
	{ Action("0704", at_response, "0a7a454741544501010101") },
	{ Action("07fe", at_send_BC, "07ff00") },
	{ Action("b505", at_doNothing, "") },
	{ Action("b516", at_doNothing, "") } };

	int findAction(const EbusSequence& eSeq);
	bool createResponse(EbusSequence& eSeq);
	bool createMessage(const unsigned char source, const unsigned char target, EbusSequence& eSeq);

};

#endif // EBUS_REACTION_H
