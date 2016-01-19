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

#include "NetworkDevice.h"

#include <cstring>

#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>

NetworkDevice::~NetworkDevice()
{
	closeDevice();
}

int NetworkDevice::openDevice(const string device, const bool noDeviceCheck)
{
	m_noDeviceCheck = noDeviceCheck;

	struct sockaddr_in sock;
	char* hostport;
	int ret;

	m_open = false;

	memset((char*) &sock, 0, sizeof(sock));

	hostport = strdup(device.c_str());
	const char* host = strtok(hostport, ":");
	const char* port = strtok(NULL, ":");

	if (inet_addr(host) == INADDR_NONE)
	{
		struct hostent* he;

		he = gethostbyname(host);
		if (he == NULL)
		{
			free(hostport);
			return (DEV_ERR_OPEN);
		}

		memcpy(&sock.sin_addr, he->h_addr_list[0], he->h_length);
	}
	else
	{
		ret = inet_aton(host, &sock.sin_addr);
		if (ret == 0)
		{
			free(hostport);
			return (DEV_ERR_OPEN);
		}
	}

	sock.sin_family = AF_INET;
	sock.sin_port = htons(strtol(port, NULL, 10));

	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd < 0)
	{
		free(hostport);
		return (DEV_ERR_OPEN);
	}

	ret = connect(m_fd, (struct sockaddr*) &sock, sizeof(sock));
	if (ret < 0)
	{
		free(hostport);
		return (DEV_ERR_OPEN);
	}

	free(hostport);
	m_open = true;

	return (DEV_OK);
}

void NetworkDevice::closeDevice()
{
	if (m_open == true)
	{
		// close file descriptor from network device
		close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}

