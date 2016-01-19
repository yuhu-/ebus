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

#include "TCPClient.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

TCPSocket* TCPClient::connect(const string& server, const int& port)
{
	struct sockaddr_in address;
	int ret;

	memset((char*) &address, 0, sizeof(address));

	if (inet_addr(server.c_str()) == INADDR_NONE)
	{
		struct hostent* he;

		he = gethostbyname(server.c_str());
		if (he == NULL) return (NULL);

		memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
	}
	else
	{
		ret = inet_aton(server.c_str(), &address.sin_addr);
		if (ret == 0) return (NULL);
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);

	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0) return (NULL);

	ret = ::connect(sfd, (struct sockaddr*) &address, sizeof(address));
	if (ret < 0) return (NULL);

	return (new TCPSocket(sfd, &address));
}

