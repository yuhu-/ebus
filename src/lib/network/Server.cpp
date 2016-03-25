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

#include "Server.h"

#include <cstring>
#include <sstream>

#include <unistd.h>
#include <arpa/inet.h>

using std::ostringstream;

Server::Server(const string& address, const int port, const bool udp)
	: m_address(address), m_port(port), m_udp(udp)
{
}

Server::~Server()
{
	if (m_sfd > 0) close(m_sfd);
}

int Server::start()
{
	if (m_ready == true) return (0);

	if (m_udp == true)
		m_sfd = socket(AF_INET, SOCK_DGRAM, 0);
	else
		m_sfd = socket(AF_INET, SOCK_STREAM, 0);

	if (m_sfd < 0) return (m_sfd);

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	server.sin_family = AF_INET;
	server.sin_port = htons(m_port);

	if (m_address.size() > 0)
		inet_pton(AF_INET, m_address.c_str(), &(server.sin_addr));
	else
		server.sin_addr.s_addr = INADDR_ANY;

	int optval = 1;
	setsockopt(m_sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	int result = bind(m_sfd, (struct sockaddr*) &server, sizeof(server));
	if (result < 0) return (result);

	if (m_udp == false)
	{
		result = listen(m_sfd, 5);
		if (result < 0) return (result);
	}

	m_ready = true;
	return (result);
}

Socket* Server::newSocket()
{

	if (m_ready == false) return (nullptr);

	struct sockaddr_in address;
	socklen_t len = sizeof(address);

	memset(&address, 0, sizeof(address));

	if (m_udp == true)
	{
		return (new Socket(m_sfd, &address));
	}
	else
	{
		int sfd = accept(m_sfd, (struct sockaddr*) &address, &len);
		if (sfd < 0) return (nullptr);

		return (new Socket(sfd, &address));
	}
}

int Server::getFD() const
{
	return (m_sfd);
}

string Server::toString() const
{
	ostringstream ostr;

	ostr << m_address << ":" << m_port;

	return (ostr.str());
}

