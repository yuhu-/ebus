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

#include <SerialDevice.h>

#include <cstring>

#include <unistd.h>
#include <fcntl.h>

ebusfsm::SerialDevice::~SerialDevice()
{
	closeDevice();
}

int ebusfsm::SerialDevice::openDevice(const std::string& device, const bool devicecheck)
{
	m_deviceCheck = devicecheck;
	struct termios newSettings;
	m_open = false;

	// open file descriptor
	m_fd = open(device.c_str(), O_RDWR | O_NOCTTY);

	if (m_fd < 0 || isatty(m_fd) == 0) return (DEV_ERR_OPEN);

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
	return (DEV_OK);
}

void ebusfsm::SerialDevice::closeDevice()
{
	if (m_open == true)
	{
		// empty device buffer
		tcflush(m_fd, TCIOFLUSH);

		// activate old settings of serial device
		tcsetattr(m_fd, TCSANOW, &m_oldSettings);

		// close file descriptor from serial device
		close(m_fd);

		m_fd = -1;
		m_open = false;
	}
}
