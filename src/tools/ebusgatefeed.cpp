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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Option.h"
#include "EbusDevice.h"

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>

#include <unistd.h>

using std::fstream;
using std::ios;
using std::cout;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;

Option& O = Option::getOption("/path/dumpfile");

void define_args()
{
	O.setVersion("ebusgatefeed is part of " "" PACKAGE_STRING"");

	O.addText(" 'ebusgatefeed' sends hex values from dump file to a pseudo terminal device (pty)\n\n"
		"   Usage: 1. 'socat -d -d pty,raw,echo=0 pty,raw,echo=0'\n"
		"          2. create symbol links to appropriate devices\n"
		"             for example: 'ln -s /dev/pts/5 /dev/ttyUSB5'\n"
		"                          'ln -s /dev/pts/6 /dev/ttyUSB6'\n"
		"          3. start ebusgate: 'ebusgate -f -n -d /dev/ttyUSB5'\n"
		"          4. start ebusgatefeed: 'ebusgatefeed -d /dev/ttyUSB6 /path/to/ebus_dump.bin'\n\n"
		"Options:\n");

	O.addString("device", "d", "/dev/ttyUSB", ot_mandatory, "link on pseudo terminal device (/dev/ttyUSB)");

	O.addLong("time", "t", 10000, ot_mandatory, "delay between 2 bytes in 'us' (10000)");

}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	if (O.parseArgs(argc, argv) == false) return (EXIT_SUCCESS);

	if (O.missingCommand() == true)
	{
		cout << "ebus dump file is required." << endl;
		exit(EXIT_FAILURE);
	}

	EbusDevice device(O.getString("device"), true);

	device.open();
	if (device.isOpen() == true)
	{
		cout << "open successful." << endl;

		fstream file(O.getCommand().c_str(), ios::in | ios::binary);

		if (file.is_open() == true)
		{

			while (file.eof() == false)
			{
				unsigned char byte = file.get();
				cout << hex << setw(2) << setfill('0') << static_cast<unsigned>(byte) << endl;

				device.send(byte);
				usleep(O.getLong("time"));
			}

			file.close();
		}
		else
		{
			cout << "error opening file " << O.getString("file") << endl;
		}

		device.close();
		if (device.isOpen() == false) cout << "close successful." << endl;
	}
	else
	{
		cout << "error opening device " << O.getString("device") << endl;
	}

	exit(EXIT_SUCCESS);
}

