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

#ifndef LIBEBUS_EBUSDEVICE_H
#define LIBEBUS_EBUSDEVICE_H

#include "Device.h"

#include <queue>
#include <memory>

using std::unique_ptr;

namespace libebus
{

class EbusDevice
{

	enum class Type
	{
		serial, network
	};

public:
	EbusDevice(const string& device, const bool deviceCheck);

	int open();
	void close();

	bool isOpen();

	ssize_t send(const unsigned char value);
	ssize_t recv(unsigned char& value, const long sec, const long nsec);

	const string errorText(const int error) const;

private:
	const string m_deviceName;

	unique_ptr<Device> m_device = nullptr;

	bool m_deviceCheck;

	void setType(const Type type);

};

} // namespace libebus

#endif // LIBEBUS_EBUSDEVICE_H
