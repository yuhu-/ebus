/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#include <Device.h>

#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

ebusfsm::Device::~Device()
{
}

bool ebusfsm::Device::isOpen()
{
	if (isValid() == false) m_open = false;

	return (m_open);
}

ssize_t ebusfsm::Device::send(const unsigned char value)
{
	if (isValid() == false) return (DEV_ERR_VALID);

	// write byte to device
	int ret = write(m_fd, &value, 1);
	if (ret == -1) return(DEV_ERR_SEND);

	return (DEV_OK);
}

ssize_t ebusfsm::Device::recv(unsigned char& value, const long sec, const long nsec)
{
	if (isValid() == false) return (DEV_ERR_VALID);

	if (sec > 0 || nsec > 0)
	{
		int ret;
		struct timespec tdiff =
		{ sec, nsec * 1000L };

		int nfds = 1;
		struct pollfd fds[nfds];

		std::memset(fds, 0, sizeof(struct pollfd) * nfds);

		fds[0].fd = m_fd;
		fds[0].events = POLLIN;

		ret = ppoll(fds, nfds, &tdiff, nullptr);

		if (ret == -1) return (DEV_ERR_POLL);
		if (ret == 0) return (DEV_WRN_TIMEOUT);
	}

	// read byte from device
	ssize_t nbytes = read(m_fd, &value, 1);
	if (nbytes < 0) return (DEV_ERR_READ);
	if (nbytes == 0) return (DEV_WRN_EOF);

	return (DEV_OK);
}

bool ebusfsm::Device::isValid()
{
	if (m_deviceCheck == true)
	{
		int port;

		if (ioctl(m_fd, TIOCMGET, &port) == -1)
		{
			closeDevice();
			m_open = false;
			return (false);
		}
	}

	return (true);
}
