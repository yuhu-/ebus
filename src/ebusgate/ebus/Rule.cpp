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

#include "Rule.h"

#include <cstring>
#include <map>
#include <iomanip>

using std::ostringstream;
using std::map;

map<RuleType, string> RuleTypeNames =
{
{ rt_ignore, "I" },
{ rt_response, "R" },
{ rt_send_BC, "BC" },
{ rt_send_MM, "MM" },
{ rt_send_MS, "MS" } };

RuleType findType(const string& item)
{
	for (const auto& rule : RuleTypeNames)
		if (strcasecmp(rule.second.c_str(), item.c_str()) == 0) return (rule.first);

	return (rt_undefined);
}

int Rule::uniqueID = 1;

Rule::Rule(const Sequence& seq, RuleType type, const string& message)
	: m_id(uniqueID++), m_seq(seq), m_type(type), m_message(message)
{
}

int Rule::getID() const
{
	return (m_id);
}

RuleType Rule::getType() const
{
	return (m_type);
}

string Rule::getMessage() const
{
	return (m_message);
}

bool Rule::equal(const Sequence& seq)
{
	if (m_seq.compare(seq) == 0) return (true);

	return (false);
}

bool Rule::match(const Sequence& seq)
{
	if (seq.find(m_seq) != Sequence::npos) return (true);

	return (false);
}

const string Rule::toString()
{
	ostringstream ostr;

	ostr << "id: " << m_id << " filter: " << m_seq.toString() << " rule: " << RuleTypeNames[m_type] << " message: "
		<< m_message;

	return (ostr.str());
}

