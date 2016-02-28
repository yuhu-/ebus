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

Socket* Client::newSocket(const string& address, const int& port, const bool& udp)
{
	int ret;

	memset((char*) &m_client, 0, sizeof(m_client));

	if (inet_addr(address.c_str()) == INADDR_NONE)
	{
		struct hostent* he;

		he = gethostbyname(address.c_str());
		if (he == nullptr) return (nullptr);

		memcpy(&m_client.sin_addr, he->h_addr_list[0], he->h_length);
	}
	else
	{
		ret = inet_aton(address.c_str(), &m_client.sin_addr);
		if (ret == 0) return (nullptr);
	}

	m_client.sin_family = AF_INET;
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

