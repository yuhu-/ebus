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

#include "ProcessBase.h"

#include <map>
#include <cstring>

using std::map;

map<ProcessType, string> ProcessTypeNames =
{
{ pt_undefined, "U" },
{ pt_ignore, "I" },
{ pt_response, "R" },
{ pt_send, "S" } };

ProcessType findType(const string& str)
{
	for (const auto& rule : ProcessTypeNames)
		if (strcasecmp(rule.second.c_str(), str.c_str()) == 0) return (rule.first);

	return (pt_undefined);
}

const string getTypeName(ProcessType type)
{
	return (ProcessTypeNames[type]);
}

bool find(const Sequence& seq, const string& str)
{
	if (seq.find(Sequence(str)) != Sequence::npos) return (true);

	return (false);
}

