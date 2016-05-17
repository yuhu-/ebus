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

#include "Filter.h"

#include <sstream>

using std::ostringstream;

int Filter::uniqueID = 1;

Filter::Filter(const Sequence& seq)
	: m_id(uniqueID++), m_seq(seq)
{
}

int Filter::getID() const
{
	return (m_id);
}

Sequence Filter::getFilter() const
{
	return (m_seq);
}

bool Filter::equal(const Sequence& seq)
{
	if (m_seq.compare(seq) == 0) return (true);

	return (false);
}

bool Filter::match(const Sequence& seq)
{
	if (seq.find(m_seq) != Sequence::npos) return (true);

	return (false);
}

const string Filter::toString()
{
	ostringstream ostr;

	ostr << "id: " << m_id << " filter: " << m_seq.toString();

	return (ostr.str());
}

