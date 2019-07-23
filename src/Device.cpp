/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include "Device.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>

#include "runtime_warning.h"

ebus::Device::Device(const std::string &device) : m_device(device)
{
}

ebus::Device::~Device()
{
	close();
}

void ebus::Device::open()
{
	if (!m_open)
	{
		struct termios newSettings;
		m_open = false;

		// open file descriptor
		m_fd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY);
		if (m_fd < 0 || isatty(m_fd) == 0) throw std::runtime_error("An error occurred while opening the ebus device");

		// save current settings
		tcgetattr(m_fd, &m_oldSettings);

		// create new settings
		std::memset(&newSettings, '\0', sizeof(newSettings));

		newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
		newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // non-canonical mode
		newSettings.c_iflag |= IGNPAR; // ignore parity errors
		newSettings.c_oflag &= ~OPOST;

		// non-canonical mode: read() blocks until at least one byte is available
		newSettings.c_cc[VMIN] = 1;
		newSettings.c_cc[VTIME] = 0;

		// empty device buffer
		tcflush(m_fd, TCIFLUSH);

		// activate new settings of serial device
		tcsetattr(m_fd, TCSAFLUSH, &newSettings);

		// set serial device into blocking mode
		fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) & ~O_NONBLOCK);

		m_open = true;
	}
}

void ebus::Device::close()
{
	if (m_open)
	{
		// empty device buffer
		tcflush(m_fd, TCIOFLUSH);

		// activate old settings of serial device
		tcsetattr(m_fd, TCSANOW, &m_oldSettings);

		// close file descriptor from serial device
		::close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}

bool ebus::Device::isOpen()
{
	return (m_open);
}

void ebus::Device::send(const std::byte byte)
{
	isValid();

	// write byte to device
	int ret = write(m_fd, &byte, 1);
	if (ret == -1) throw std::runtime_error("An device error occurred while sending data");
}

void ebus::Device::recv(std::byte &byte, const long sec, const long nsec)
{
	isValid();

	if (sec > 0 || nsec > 0)
	{
		int ret;
		struct timespec tdiff =
		{ sec, nsec * 1000L };

		const nfds_t nfds = 1;
		struct pollfd fds[nfds];

		std::memset(fds, 0, sizeof(struct pollfd) * nfds);

		fds[0].fd = m_fd;
		fds[0].events = POLLIN;

		ret = ppoll(fds, nfds, &tdiff, nullptr);

		if (ret == -1) throw std::runtime_error("An device error occurred while waiting on ppoll");
		if (ret == 0) throw ebus::runtime_warning("A timeout occurred while waiting for incoming data");
	}

	// read byte from device
	ssize_t nbytes = read(m_fd, &byte, 1);
	if (nbytes < 0) throw std::runtime_error("An error occurred while reading file descriptor");
	if (nbytes == 0) throw ebus::runtime_warning("An EOF occurred while data was being received");
}

bool ebus::Device::isValid()
{
	int port;

	if (ioctl(m_fd, TIOCMGET, &port) == -1)
	{
		close();
		throw std::runtime_error("The file descriptor of the ebus device is invalid");
	}

	return (true);
}
