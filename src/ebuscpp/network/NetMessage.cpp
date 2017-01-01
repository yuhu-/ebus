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

#include "NetMessage.h"

NetMessage::NetMessage(const string& data, const string& ip, const long port)
	: Notify(), m_data(data), m_ip(ip), m_port(port)
{
}

string NetMessage::getData() const
{
	return (m_data);
}

string NetMessage::getResult() const
{
	return (m_result);
}

void NetMessage::setResult(const string result)
{
	m_result = result;
}

string NetMessage::getIP() const
{
	return (m_ip);
}

long NetMessage::getPort() const
{
	return (m_port);
}
