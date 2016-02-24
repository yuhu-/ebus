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
#include "TCPClient.h"

#include <iostream>
#include <sstream>

#include <unistd.h>
#include <poll.h>

using std::ostringstream;
using std::cin;
using std::cout;
using std::endl;

Option& O = Option::getOption("Command", "{Args...}");

void define_args()
{
	O.setVersion("ebusgatectl is part of " "" PACKAGE_STRING"");

	O.addText(" 'ebusgatectl' is a tcp socket client for ebusgate.\n\n"
		"   hint: try 'help' for available ebusgate commands.\n\n"
		"Options:\n");

	O.addString("server", "s", "localhost", ot_mandatory, "name or ip (localhost)");

	O.addLong("port", "p", 8888, ot_mandatory, "port (8888)");

}

string fetchData(TCPSocket* socket, bool& listening)
{
	char data[1024];
	ssize_t datalen;
	ostringstream ss;
	string message;

	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 0;
	tdiff.tv_nsec = 1E8;

	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(struct pollfd) * nfds);

	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	fds[1].fd = socket->getFD();
	fds[1].events = POLLIN;

	while (true)
	{

		// wait for new fd event
		int ret = ppoll(fds, nfds, &tdiff, nullptr);

		bool newData = false;
		bool newInput = false;
		if (ret != 0)
		{
			// new data from notify
			newInput = fds[0].revents & POLLIN;

			// new data from socket
			newData = fds[1].revents & POLLIN;
		}

		if (newData == true)
		{
			if (socket->isValid() == true)
			{
				datalen = socket->recv(data, sizeof(data));

				if (datalen < 0)
				{
					perror("recv");
					break;
				}

				for (int i = 0; i < datalen; i++)
					ss << data[i];

				if ((ss.str().length() >= 2 && ss.str()[ss.str().length() - 2] == '\n'
					&& ss.str()[ss.str().length() - 1] == '\n') || listening == true) break;

			}
			else
			{
				break;
			}

		}
		else if (newInput == true)
		{
			getline(cin, message);
			message += '\n';

			socket->send(message.c_str(), message.size());

			if (strncasecmp(message.c_str(), "QUIT", 4) == 0
				|| strncasecmp(message.c_str(), "STOP", 4) == 0) exit(EXIT_SUCCESS);

			message.clear();
		}

	}

	return (ss.str());
}

bool connect(const string& host, const int& port, bool once)
{

	TCPClient* client = new TCPClient();
	TCPSocket* socket = client->connect(host, port);

	if (socket != nullptr)
	{
		do
		{
			string message;

			if (once == false)
			{
				cout << host << ": ";
				getline(cin, message);
			}
			else
			{
				message = O.getCommand();
				for (int i = 0; i < O.numArgs(); i++)
				{
					message += " ";
					message += O.getArg(i);
				}
			}

			socket->send(message.c_str(), message.size());

			if (strncasecmp(message.c_str(), "QUIT", 4) != 0
				&& strncasecmp(message.c_str(), "STOP", 4) != 0)
			{
				bool listening = false;

				if (strncasecmp(message.c_str(), "LISTEN", 6) == 0)
				{
					listening = true;
					while (listening)
					{
						string result(fetchData(socket, listening));
						cout << result;
						if (strncasecmp(result.c_str(), "LISTEN STOPPED", 14) == 0) break;
					}
				}
				else
				{
					cout << fetchData(socket, listening);
				}
			}
			else
			{
				break;
			}

		} while (once == false);

		delete socket;
	}
	else
	{
		cout << "error connecting to " << host << ":" << port << endl;
	}

	delete client;

	if (once == false) return (false);

	return (true);
}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	if (O.parseArgs(argc, argv) == false) exit(EXIT_FAILURE);

	if (O.missingCommand() == true)
		connect(O.getString("server"), O.getLong("port"), false);
	else
		connect(O.getString("server"), O.getLong("port"), true);

	exit(EXIT_SUCCESS);
}

