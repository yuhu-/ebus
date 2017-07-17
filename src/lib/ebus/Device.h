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

#define DEV_WRN_EOF      2  // An 'EOF' occurred while data was being received
#define DEV_WRN_TIMEOUT  1  // A timeout occurred while waiting for incoming data

#define DEV_OK           0  // success

#define DEV_ERR_OPEN    -1  // An error occurred while opening the 'eBus' device
#define DEV_ERR_VALID   -2  // The file descriptor of the 'eBus' device is invalid
#define DEV_ERR_RECV    -3  // An device error occurred while receiving data
#define DEV_ERR_SEND    -4  // An device error occurred while sending data
#define DEV_ERR_POLL    -5  // An device error occurred while waiting on 'ppoll'

class Device
{

public:
	virtual ~Device();

	virtual int openDevice(const string& device, const bool deviceCheck) = 0;
	virtual void closeDevice() = 0;

	bool isOpen();

	ssize_t send(const unsigned char value);
	ssize_t recv(unsigned char& value, const long sec, const long nsec);

	static const string errorText(const int error);

protected:
	int m_fd = -1;

	bool m_open = false;

	bool m_deviceCheck = true;

private:
	bool isValid();

};

} // namespace libebus

#endif // LIBEBUS_DEVICE_H
