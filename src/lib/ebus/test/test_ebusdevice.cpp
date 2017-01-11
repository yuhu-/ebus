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

#include <iostream>
#include <iomanip>

using libebus::EbusDevice;
using std::cout;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;

int main()
{
	string dev("/dev/ttyUSB5");
	EbusDevice device(dev, true);

	device.open();

	if (device.isOpen() == true) cout << "openPort successful." << endl;

	int count = 0;

	while (true)
	{
		int ret;
		unsigned char byte = 0;
		ret = device.recv(byte, 0, 0);

		if (ret == DEV_OK) cout << hex << setw(2) << setfill('0') << static_cast<unsigned>(byte) << endl;

		count++;
	}

	device.close();

	if (device.isOpen() == false) cout << "closePort successful." << endl;

	return (0);

}
