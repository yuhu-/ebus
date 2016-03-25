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

#ifndef EBUS_ACTION_H
#define EBUS_ACTION_H

#include "Sequence.h"

enum ActionType
{
	at_notDefined,	// not defined
	at_doNothing,	// do nothing
	at_response,	// prepare slave part and send response
	at_send_BC,	// create and send broadcast message
	at_send_MM,	// create and send master master message
	at_send_MS	// create and send master slave message
};

class Action
{

public:
	Action(const string& search, ActionType type, const string& message);

	ActionType getType() const;
	string getMessage() const;

	bool match(const Sequence& seq);

private:
	Sequence m_search;
	ActionType m_type;
	string m_message;

};

#endif // EBUS_ACTION_H

