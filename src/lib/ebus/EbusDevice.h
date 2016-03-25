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

#ifndef LIBEBUS_EBUSDEVICE_H
#define LIBEBUS_EBUSDEVICE_H

#include "Device.h"

#include <queue>

enum DeviceType
{
	dt_serial, dt_network
};

class EbusDevice
{

public:
	EbusDevice(const string& device, const bool noDeviceCheck);
	~EbusDevice();

	int open();
	void close();

	bool isOpen();

	ssize_t send(const unsigned char value);
	ssize_t recv(unsigned char& value, const long sec, const long nsec);

	const string errorText(const int error) const;

private:
	const string m_deviceName;

	Device* m_device;

	bool m_noDeviceCheck;

	void setType(const DeviceType type);

};

#endif // LIBEBUS_EBUSDEVICE_H
