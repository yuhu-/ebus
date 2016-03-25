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

#ifndef LIBNETWORK_SERVER_H
#define LIBNETWORK_SERVER_H

#include "Socket.h"

class Server
{

public:
	Server(const string& address, const int port, const bool udp = false);
	~Server();

	int start();

	Socket* newSocket();

	int getFD() const;

	string toString() const;

private:
	int m_sfd = 0;

	string m_address;

	int m_port;

	bool m_udp;

	bool m_ready = false;

};

#endif // LIBNETWORK_SERVER_H

