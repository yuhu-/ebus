/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Options.h"
#include "EbusDevice.h"

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>

#include <unistd.h>

using libutils::Options;
using std::fstream;
using std::ios;
using std::cout;
using std::endl;
using std::hex;
using std::setw;
using std::setfill;

void define_args()
{
	Options& options = Options::getOption("/path/dumpfile");

	options.setVersion("ebusfeed is part of " "" PACKAGE_STRING"");

	options.addDescription(" 'ebusfeed' sends hex values from dump file to a pseudo terminal device (pty)\n\n"
		"  Example: 1. 'socat -d -d pty,raw,echo=0 pty,raw,echo=0'\n"
		"           2. create symbol links to appropriate devices\n"
		"              for example: 'ln -s /dev/pts/5 /dev/ttyUSB5'\n"
		"                           'ln -s /dev/pts/6 /dev/ttyUSB6'\n"
		"           3. start ebusproxy: 'ebusproxy -f -n -d /dev/ttyUSB5'\n"
		"           4. start ebusfeed: 'ebusfeed -d /dev/ttyUSB6 /path/to/ebus_dump.bin'");

	options.addString("device", "d", "/dev/ttyUSB", "link on pseudo terminal device (/dev/ttyUSB)");

	options.addLong("time", "t", 10000, "delay between 2 bytes in 'us' (10000)");

}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	Options& options = Options::getOption();
	if (options.parse(argc, argv) == false) return (EXIT_SUCCESS);

	if (options.missingCommand() == true)
	{
		cout << "ebus dump file is required." << endl;
		exit(EXIT_FAILURE);
	}

	EbusDevice device(options.getString("device"), true);

	device.open();
	if (device.isOpen() == true)
	{
		cout << "open successful." << endl;

		fstream file(options.getCommand().c_str(), ios::in | ios::binary);

		if (file.is_open() == true)
		{

			while (file.eof() == false)
			{
				unsigned char byte = file.get();
				cout << hex << setw(2) << setfill('0') << static_cast<unsigned>(byte) << endl;

				device.send(byte);
				usleep(options.getLong("time"));
			}

			file.close();
		}
		else
		{
			cout << "error opening file " << options.getString("file") << endl;
		}

		device.close();
		if (device.isOpen() == false) cout << "close successful." << endl;
	}
	else
	{
		cout << "error opening device " << options.getString("device") << endl;
	}

	exit(EXIT_SUCCESS);
}

