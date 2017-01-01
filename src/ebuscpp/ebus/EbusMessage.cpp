/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
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

#include "EbusMessage.h"

#include <sstream>

using std::ostringstream;

EbusMessage::EbusMessage(EbusSequence& eSeq, bool intern)
	: Notify(), m_ebusSequence(eSeq), m_intern(intern)
{
}

EbusSequence& EbusMessage::getEbusSequence()
{
	return (m_ebusSequence);
}

bool EbusMessage::isIntern() const
{
	return (m_intern);
}

void EbusMessage::setResult(const string& result)
{
	m_result = result;
}

const string EbusMessage::getResult()
{
	ostringstream ostr;

	if (m_result.size() != 0)
		ostr << m_result;
	else if (m_ebusSequence.getType() == EBUS_TYPE_BC)
		ostr << "done";
	else if (m_ebusSequence.getType() == EBUS_TYPE_MM)
		ostr << m_ebusSequence.toStringSlaveACK();
	else
		ostr << m_ebusSequence.toStringSlave();

	return (ostr.str());
}

