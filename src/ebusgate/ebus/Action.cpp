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

Action::Action(const string& search, const ActionType type, const string& message)
	: m_search(Sequence(search)), m_type(type), m_message(message)
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

bool Action::match(const Sequence& seq)
{
	if (seq.find(m_search) != Sequence::npos) return (true);

	return (false);
}

