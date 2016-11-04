/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#include "Relation.h"

#include <iomanip>

using std::ostringstream;

Relation::Relation(int hostID, int filterID)
	: m_hostID(hostID), m_filterID(filterID)
{
}

int Relation::getHostID() const
{
	return (m_hostID);
}

int Relation::getFilterID() const
{
	return (m_filterID);
}

bool Relation::equal(int hostID, int filterID)
{
	if (m_hostID == hostID && m_filterID == filterID) return (true);

	return (false);
}

const string Relation::toString()
{
	ostringstream ostr;

	ostr << "host: " << m_hostID << " filter: " << m_filterID;

	return (ostr.str());
}

