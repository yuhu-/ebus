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

#include "Options.h"
#include "Client.h"
#include "Server.h"

#include <iostream>
#include <sstream>

#include <unistd.h>
#include <poll.h>

using std::ostringstream;
using std::cin;
using std::cout;
using std::endl;

void define_args()
{
	Options& O = Options::getOption("Command", "{Argument}");

	O.setVersion("ebusgatectl is part of " "" PACKAGE_STRING"");

	O.addDescription(" 'ebusgatectl' is a tcp/udp socket client for ebusgate.\n"
		"  hint: try 'help' for available ebusgate commands.");

	O.addString("server", "s", "localhost", "name or ip (localhost)");

	O.addLong("port", "p", 8888, "port (8888)");

	O.addBool("udp", "u", false, "connect via udp");
}

void connect(const string& host, const int& port, const bool& udp)
{
	Options& O = Options::getOption();

	Client* client = new Client();
	Socket* socket = client->newSocket(host, port, udp);

	if (socket != nullptr)
	{
		string message = O.getCommand();
		for (int i = 0; i < O.numArgs(); i++)
		{
			message += " ";
			message += O.getArg(i);
		}

		socket->send(message.c_str(), message.size(), client->getSock(), sizeof(struct sockaddr_in));

		if (strncasecmp(message.c_str(), "QUIT", 4) != 0 && strncasecmp(message.c_str(), "STOP", 4) != 0)
		{
			struct sockaddr_in sock;
			socklen_t socklen = sizeof(struct sockaddr_in);
			char data[1024];
			ssize_t datalen = socket->recv(data, sizeof(data) - 1, &sock, &socklen);
			data[datalen] = '\0';
			cout << data;
		}

		delete socket;
	}
	else
	{
		cout << "error connecting to " << host << ":" << port << endl;
	}

	delete client;
}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	Options& O = Options::getOption();
	if (O.parse(argc, argv) == false) exit(EXIT_FAILURE);

	connect(O.getString("server"), O.getLong("port"), O.getBool("udp"));

	exit(EXIT_SUCCESS);
}

