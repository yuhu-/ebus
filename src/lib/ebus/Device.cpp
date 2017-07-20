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

#include "Device.h"

#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

using std::ostringstream;

libebus::Device::~Device()
{
}

bool libebus::Device::isOpen()
{
	if (isValid() == false) m_open = false;

	return (m_open);
}

ssize_t libebus::Device::send(const unsigned char value)
{
	if (isValid() == false) return (DEV_ERR_VALID);

	// write bytes to device
	return (write(m_fd, &value, 1) == 1 ? DEV_OK : DEV_ERR_SEND);
}

ssize_t libebus::Device::recv(unsigned char& value, const long sec, const long nsec)
{
	if (isValid() == false) return (DEV_ERR_VALID);

	if (sec > 0 || nsec > 0)
	{
		int ret;
		struct timespec tdiff;

		// set select timeout
		tdiff.tv_sec = sec;
		tdiff.tv_nsec = nsec * 1000;

		int nfds = 1;
		struct pollfd fds[nfds];

		memset(fds, 0, sizeof(struct pollfd) * nfds);

		fds[0].fd = m_fd;
		fds[0].events = POLLIN;

		ret = ppoll(fds, nfds, &tdiff, nullptr);

		if (ret == -1) return (DEV_ERR_POLL);
		if (ret == 0) return (DEV_WRN_TIMEOUT);
	}

	// directly read byte from device
	ssize_t nbytes = read(m_fd, &value, 1);
	if (nbytes == 0) return (DEV_WRN_EOF);

	return (nbytes < 0 ? DEV_ERR_POLL : DEV_OK);
}

bool libebus::Device::isValid()
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
