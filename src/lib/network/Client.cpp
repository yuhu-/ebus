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

#include "Client.h"

#include <cstring>

#include <arpa/inet.h>

Socket* Client::newSocket(const string& address, const int port, const bool udp)
{
	int ret;
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof hints);
	memset((char*) &m_client, 0, sizeof(m_client));

	hints.ai_family = AF_INET;

	if (udp == true)
		hints.ai_socktype = SOCK_DGRAM;
	else
		hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(address.c_str(), nullptr, &hints, &servinfo);
	if (ret < 0) return (nullptr);

	m_client = *(reinterpret_cast<sockaddr_in*>(servinfo->ai_addr));

	freeaddrinfo(servinfo);

	m_client.sin_port = htons(port);

	int sfd;

	if (udp == true)
	{
		sfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sfd < 0) return (nullptr);
	}
	else
	{
		sfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sfd < 0) return (nullptr);

		ret = connect(sfd, (struct sockaddr*) &m_client, sizeof(m_client));
		if (ret < 0) return (nullptr);
	}

	return (new Socket(sfd, &m_client));
}

const struct sockaddr_in* Client::getSock()
{
	return (&m_client);
}

