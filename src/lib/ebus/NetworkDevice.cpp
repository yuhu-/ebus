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

	struct sockaddr_in address;
	int ret;

	m_open = false;

	memset((char*) &address, 0, sizeof(address));

	const string host = device.substr(0, device.find(":"));
	const string port = device.substr(device.find(":") + 1);

	if (inet_addr(host.c_str()) == INADDR_NONE)
	{
		struct hostent* he;

		he = gethostbyname(host.c_str());
		if (he == nullptr) return (DEV_ERR_OPEN);

		memcpy(&address.sin_addr, he->h_addr_list[0], he->h_length);
	}
	else
	{
		ret = inet_aton(host.c_str(), &address.sin_addr);
		if (ret == 0) return (DEV_ERR_OPEN);
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(strtol(port.c_str(), nullptr, 10));

	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd < 0) return (DEV_ERR_OPEN);

	ret = connect(m_fd, (struct sockaddr*) &address, sizeof(address));
	if (ret < 0) return (DEV_ERR_OPEN);

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

