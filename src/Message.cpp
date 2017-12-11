/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#include <base/Message.h>

#include <sstream>

ebusfsm::Message::Message(EbusSequence& eSeq)
	: Notify(), m_ebusSequence(eSeq)
{
}

ebusfsm::EbusSequence& ebusfsm::Message::getEbusSequence()
{
	return (m_ebusSequence);
}

void ebusfsm::Message::setState(int state)
{
	m_state = state;
}

int ebusfsm::Message::getState() const
{
	return (m_state);
}
