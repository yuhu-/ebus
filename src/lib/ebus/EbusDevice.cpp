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

#include "EbusDevice.h"
#include "SerialDevice.h"
#include "NetworkDevice.h"

#include <cstring>
#include <sstream>

using std::ostringstream;

EbusDevice::EbusDevice(const string deviceName, const bool noDeviceCheck)
	: m_deviceName(deviceName), m_noDeviceCheck(noDeviceCheck)
{
	m_device = NULL;

	if (strchr(deviceName.c_str(), '/') == NULL
		&& strchr(deviceName.c_str(), ':') != NULL)
	{
		setType(dt_network);
		m_noDeviceCheck = true;
	}
	else
	{
		setType(dt_serial);
	}
}

EbusDevice::~EbusDevice()
{
	delete m_device;
}

int EbusDevice::open()
{
	return (m_device->openDevice(m_deviceName, m_noDeviceCheck));
}

void EbusDevice::close()
{
	m_device->closeDevice();
}

bool EbusDevice::isOpen()
{
	return (m_device->isOpen());
}

ssize_t EbusDevice::send(const unsigned char value)
{
	return (m_device->send(value));
}

ssize_t EbusDevice::recv(unsigned char& value, const long sec, const long nsec)
{
	return (m_device->recv(value, sec, nsec));
}

const char* EbusDevice::getDeviceName()
{
	return (m_deviceName.c_str());
}

const char* EbusDevice::getErrorText(const int error)
{
	ostringstream result;

	switch (error)
	{
	case DEV_WRN_EOF:
		result << "\033[1;33mEOF during receiving reached\033[0m";
		break;
	case DEV_WRN_TIMEOUT:
		result << "\033[1;33mTimeout during receiving reached\033[0m";
		break;
	case DEV_ERR_OPEN:
		result
			<< "\033[1;31mError occurred when opening the input device "
			<< getDeviceName() << "\033[0m";
		break;
	case DEV_ERR_VALID:
		result
			<< "\033[1;31mFile descriptor of input device is invalid\033[0m";
		break;
	case DEV_ERR_RECV:
		result
			<< "\033[1;31mError occurred during data receiving\033[0m";
		break;
	case DEV_ERR_SEND:
		result << "\033[1;31mError occurred during data sending\033[0m";
		break;
	case DEV_ERR_POLL:
		result << "\033[1;31mError occurred at ppoll waiting\033[0m";
		break;
	default:
		result << "\033[1;31mUnknown error code\033[0m";
		break;
	}

	return (result.str().c_str());
}

void EbusDevice::setType(const DeviceType type)
{
	if (m_device != NULL) delete m_device;

	switch (type)
	{
	case dt_serial:
		m_device = new SerialDevice();
		break;
	case dt_network:
		m_device = new NetworkDevice();
		break;
	};
}

