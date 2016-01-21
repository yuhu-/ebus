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

#ifndef LIBEBUS_DEVICE_H
#define LIBEBUS_DEVICE_H

#include <string>

using std::string;

#define DEV_WRN_EOF      2  // 0 bytes received
#define DEV_WRN_TIMEOUT  1  // timeout during receive
#define DEV_OK           0  // success
#define DEV_ERR_OPEN    -1  // error opening device
#define DEV_ERR_VALID   -2  // invalid file descriptor
#define DEV_ERR_RECV    -3  // receive error
#define DEV_ERR_SEND    -4  // send error
#define DEV_ERR_POLL    -5  // polling error

class Device
{

public:
	virtual ~Device();

	virtual int openDevice(const string device,
		const bool noDeviceCheck) = 0;
	virtual void closeDevice() = 0;

	bool isOpen();

	ssize_t send(const unsigned char value);
	ssize_t recv(unsigned char& value, const long sec, const long nsec);

protected:
	int m_fd = -1;

	bool m_open = false;

	bool m_noDeviceCheck = false;

private:
	bool isValid();

};

#endif // LIBEBUS_DEVICE_H
