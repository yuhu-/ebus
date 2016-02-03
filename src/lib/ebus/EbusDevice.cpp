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
#include "Color.h"

#include <cstring>
#include <sstream>
#include <map>

using std::ostringstream;
using std::map;

map<int, string> DeviceErrors =
{
{ DEV_WRN_EOF, "EOF during receiving reached" },
{ DEV_WRN_TIMEOUT, "Timeout during receiving reached" },

{ DEV_ERR_OPEN, "Error occurred when opening the input device " },
{ DEV_ERR_VALID, "File descriptor of input device is invalid" },
{ DEV_ERR_RECV, "Error occurred during data receiving" },
{ DEV_ERR_SEND, "Error occurred during data sending" },
{ DEV_ERR_POLL, "Error occurred at ppoll waiting" } };

EbusDevice::EbusDevice(const string deviceName, const bool noDeviceCheck)
	: m_deviceName(deviceName), m_noDeviceCheck(noDeviceCheck)
{
	m_device = nullptr;

	if (strchr(deviceName.c_str(), '/') == nullptr
		&& strchr(deviceName.c_str(), ':') != nullptr)
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

const string EbusDevice::errorText(const int error) const
{
	ostringstream result;

	(error > DEV_OK) ? result << color::yellow : result << color::red;

	result << DeviceErrors[error];

	if (error == DEV_ERR_OPEN)
		result << m_deviceName;

	result << color::reset;

	return (result.str());
}

void EbusDevice::setType(const DeviceType type)
{
	if (m_device != nullptr) delete m_device;

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

