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

#include "EbusMessage.h"

EbusMessage::EbusMessage(EbusSequence eSeq)
	: Notify(), m_ebusSequence(eSeq)
{
}

EbusSequence& EbusMessage::getEbusSequence()
{
	return (m_ebusSequence);
}

void EbusMessage::setResult(const string result)
{
	m_result = result;
}

string EbusMessage::getResult()
{
	string result;

	if (m_result.size() != 0)
	{
		result = m_result;
	}
	else if (m_ebusSequence.getType() == EBUS_TYPE_BC)
	{
		result = "done";
	}
	else if (m_ebusSequence.getType() == EBUS_TYPE_MM)
	{
		result = m_ebusSequence.toStringSlaveACK();
	}
	else
	{
		result = m_ebusSequence.toStringSlave();
	}

	return (result);
}
