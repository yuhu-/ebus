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

#include <NetworkDevice.h>

#include <cstring>

#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>

ebusfsm::NetworkDevice::~NetworkDevice()
{
	closeDevice();
}

int ebusfsm::NetworkDevice::openDevice(const std::string& device, const bool devicecheck)
{
	m_deviceCheck = devicecheck;
	m_open = false;

	int ret;

	struct sockaddr_in address;
	std::memset((char*) &address, 0, sizeof(address));

	const std::string host = device.substr(0, device.find(':'));
	const std::string port = device.substr(device.find(':') + 1);

	struct addrinfo hints, *servinfo;
	std::memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(host.c_str(), nullptr, &hints, &servinfo);
	if (ret < 0) return (DEV_ERR_OPEN);

	address = *(reinterpret_cast<sockaddr_in*>(servinfo->ai_addr));

	freeaddrinfo(servinfo);

	address.sin_port = htons(std::strtol(port.c_str(), nullptr, 10));

	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd < 0) return (DEV_ERR_OPEN);

	ret = connect(m_fd, (struct sockaddr*) &address, sizeof(address));
	if (ret < 0) return (DEV_ERR_OPEN);

	m_open = true;

	return (DEV_OK);
}

void ebusfsm::NetworkDevice::closeDevice()
{
	if (m_open == true)
	{
		// close file descriptor from network device
		close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}
