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

#include "Host.h"

#include <iomanip>

using std::ostringstream;
using std::endl;

int Host::uniqueID = 1;

Host::Host(const string& ip, const long& port, const bool& filter)
	: m_id(uniqueID++), m_filter(filter)
{
	m_socket = m_client.newSocket(ip, port, true);
}

Host::~Host()
{
	delete m_socket;
}

int Host::getID() const
{
	return (m_id);
}

string Host::getIP() const
{
	return (m_socket->getIP());
}

long Host::getPort() const
{
	return (m_socket->getPort());
}

void Host::setFilter(const bool& filter)
{
	m_filter = filter;
}

bool Host::hasFilter() const
{
	return (m_filter);
}

bool Host::equal(const string& ip, const long& port) const
{
	return ((m_socket->getIP() == ip && m_socket->getPort() == port) ? true : false);
}

void Host::send(const string& message)
{
	if (m_socket->isValid() == true)
	{
		ostringstream ostr(message);
		ostr << endl;
		m_socket->send(ostr.str().c_str(), ostr.str().size(), m_client.getSock(), sizeof(struct sockaddr_in));
	}
}

const string Host::toString()
{
	ostringstream ostr;

	ostr << "id: " << m_id << " address: " << m_socket->getIP() << ":" << m_socket->getPort() << " filter: "
		<< (m_filter == true ? "yes" : "no");

	return (ostr.str());
}
