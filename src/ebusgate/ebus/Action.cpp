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

#include <cstring>
#include <map>

using std::map;

map<ActionType, string> ActionTypeNames =
{
{ at_ignore, "I" },
{ at_response, "R" },
{ at_send_BC, "BC" },
{ at_send_MM, "MM" },
{ at_send_MS, "MS" } };

ActionType findType(const string& item)
{
	for (const auto& actionType : ActionTypeNames)
		if (strcasecmp(actionType.second.c_str(), item.c_str()) == 0) return (actionType.first);

	return (at_undefined);
}

Action::Action(const Sequence& seq, ActionType type, const string& message)
	: m_seq(seq), m_type(type), m_message(message)
{
}

ActionType Action::getType() const
{
	return (m_type);
}

string Action::getMessage() const
{
	return (m_message);
}

bool Action::equal(const Sequence& seq)
{
	if (m_seq.compare(seq) == 0) return (true);

	return (false);
}

bool Action::match(const Sequence& seq)
{
	if (seq.find(m_seq) != Sequence::npos) return (true);

	return (false);
}

