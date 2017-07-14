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

#ifndef LIBEBUS_DEVICE_H
#define LIBEBUS_DEVICE_H

#include <string>

using std::string;

namespace libebus
{

#define DEV_WRN_EOF      2  // EOF during receiving reached
#define DEV_WRN_TIMEOUT  1  // Timeout during receiving reached

#define DEV_OK           0  // success

#define DEV_ERR_OPEN    -1  // Error occurred when opening the input device
#define DEV_ERR_VALID   -2  // File descriptor of input device is invalid
#define DEV_ERR_RECV    -3  // Error occurred during data receiving
#define DEV_ERR_SEND    -4  // Error occurred during data sending
#define DEV_ERR_POLL    -5  // Error occurred at ppoll waiting

class Device
{

public:
	virtual ~Device();

	virtual int openDevice(const string& device, const bool deviceCheck) = 0;
	virtual void closeDevice() = 0;

	bool isOpen();

	ssize_t send(const unsigned char value);
	ssize_t recv(unsigned char& value, const long sec, const long nsec);

	const string errorText(const int error) const;

protected:
	int m_fd = -1;

	bool m_open = false;

	bool m_deviceCheck = true;

private:
	bool isValid();

};

} // namespace libebus

#endif // LIBEBUS_DEVICE_H
