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

#include <EbusDevice.h>

#include <iostream>
#include <iomanip>

int main()
{
	std::string dev("/dev/ttyUSB0");
	ebusfsm::EbusDevice device(dev, true);

	device.open();

	if (device.isOpen() == true) std::cout << "openPort successful." << std::endl;

	int count = 0;

	while (count < 100)
	{
		int ret;
		unsigned char byte = 0;
		ret = device.recv(byte, 0, 0);

		if (ret == DEV_OK) std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte) << std::endl;

		count++;
	}

	device.close();

	if (device.isOpen() == false) std::cout << "closePort successful." << std::endl;

	return (0);

}
