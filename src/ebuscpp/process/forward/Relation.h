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

#ifndef PROCESS_FORWARD_RELATION_H
#define PROCESS_FORWARD_RELATION_H

#include <string>

using std::string;

class Relation
{

public:
	Relation(int hostID, int filterID);

	int getHostID() const;
	int getFilterID() const;

	bool equal(int hostID, int filterID);

	const string toString();

private:
	int m_hostID;
	int m_filterID;

};

#endif // PROCESS_FORWARD_RELATION_H

