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

#ifndef EBUS_DEVICE_H
#define EBUS_DEVICE_H

#include <termios.h>
#include <string>

namespace ebus
{

class Device
{

public:
	Device(const std::string &device);
	~Device();

	void open();
	void close();

	bool isOpen();

	void send(const std::byte byte);
	void recv(std::byte &byte, const long sec, const long nsec);

private:
	const std::string m_device;

	termios m_oldSettings =
	{ };

	int m_fd = -1;

	bool m_open = false;

	bool isValid();

};

} // namespace ebus

#endif // EBUS_DEVICE_H
