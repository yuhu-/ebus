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

#include "EbusDevice.h"
#include "SerialDevice.h"
#include "NetworkDevice.h"
#include "Color.h"

#include <cstring>
#include <sstream>
#include <map>

using std::ostringstream;
using std::map;
using std::make_unique;

map<int, string> DeviceErrors =
{
{ DEV_WRN_EOF, "EOF during receiving reached" },
{ DEV_WRN_TIMEOUT, "Timeout during receiving reached" },

{ DEV_ERR_OPEN, "Error occurred when opening the input device " },
{ DEV_ERR_VALID, "File descriptor of input device is invalid" },
{ DEV_ERR_RECV, "Error occurred during data receiving" },
{ DEV_ERR_SEND, "Error occurred during data sending" },
{ DEV_ERR_POLL, "Error occurred at ppoll waiting" } };

libebus::EbusDevice::EbusDevice(const string& deviceName, const bool deviceCheck)
	: m_deviceName(deviceName), m_deviceCheck(deviceCheck)
{
	if (deviceName.find('/') == string::npos && deviceName.find(':') != string::npos)
	{
		setType(Type::network);
		m_deviceCheck = false;
	}
	else
	{
		setType(Type::serial);
	}
}

int libebus::EbusDevice::open()
{
	return (m_device->openDevice(m_deviceName, m_deviceCheck));
}

void libebus::EbusDevice::close()
{
	m_device->closeDevice();
}

bool libebus::EbusDevice::isOpen()
{
	return (m_device->isOpen());
}

ssize_t libebus::EbusDevice::send(const unsigned char value)
{
	return (m_device->send(value));
}

ssize_t libebus::EbusDevice::recv(unsigned char& value, const long sec, const long nsec)
{
	return (m_device->recv(value, sec, nsec));
}

const string libebus::EbusDevice::errorText(const int error) const
{
	ostringstream ostr;

	(error > DEV_OK) ? ostr << libutils::color::yellow : ostr << libutils::color::red;

	ostr << DeviceErrors[error];

	if (error == DEV_ERR_OPEN) ostr << m_deviceName;

	ostr << libutils::color::reset;

	return (ostr.str());
}

void libebus::EbusDevice::setType(const Type type)
{
	if (m_device != nullptr) m_device.reset();

	if (type == Type::serial)
		m_device = make_unique<SerialDevice>();
	else
		m_device = make_unique<NetworkDevice>();

}

