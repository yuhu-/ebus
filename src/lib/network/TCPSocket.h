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

#ifndef NETWORK_TCPSOCKET_H
#define NETWORK_TCPSOCKET_H

#include <string>

#include <sys/socket.h>

using std::string;

class TCPSocket
{
	friend class TCPClient;
	friend class TCPServer;

public:
	~TCPSocket();

	ssize_t send(const char* buffer, size_t len);
	ssize_t recv(char* buffer, size_t len);

	int getPort() const;

	string getIP() const;

	int getFD() const;

	bool isValid();

private:
	int m_sfd;

	int m_port;

	string m_ip;

	TCPSocket(int sfd, struct sockaddr_in* address);

};

#endif // NETWORK_TCPSOCKET_H

