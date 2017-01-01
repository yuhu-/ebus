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

#ifndef LIBNETWORK_CLIENT_H
#define LIBNETWORK_CLIENT_H

#include "Socket.h"

#include <netdb.h>

class Client
{

public:
	Socket* newSocket(const string& address, const int port, const bool udp = false);

	const struct sockaddr_in* getSock();

private:
	struct sockaddr_in m_client;

};

#endif // LIBNETWORK_CLIENT_H

