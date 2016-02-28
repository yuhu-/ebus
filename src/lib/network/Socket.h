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

#ifndef LIBNETWORK_SOCKET_H
#define LIBNETWORK_SOCKET_H

#include <string>

#include <sys/socket.h>

using std::string;

class Socket
{
	friend class Client;
	friend class Server;

public:
	~Socket();

	ssize_t send(const char* buffer, size_t len, const struct sockaddr_in* address, const socklen_t addrlen);
	ssize_t recv(char* buffer, size_t len, struct sockaddr_in* address, socklen_t* addrlen);

	int getPort() const;

	string getIP() const;

	int getFD() const;

	bool isValid();

private:
	int m_sfd;

	string m_ip;

	int m_port;

	Socket(int sfd, struct sockaddr_in* address);

};

#endif // LIBNETWORK_SOCKET_H

