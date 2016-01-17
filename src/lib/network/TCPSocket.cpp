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

#include "TCPSocket.h"

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

TCPSocket::~TCPSocket()
{
	close(m_sfd);
}

ssize_t TCPSocket::send(const char* buffer, size_t len)
{
	return (::send(m_sfd, buffer, len, MSG_NOSIGNAL));
}
ssize_t TCPSocket::recv(char* buffer, size_t len)
{
	return (::recv(m_sfd, buffer, len, 0));
}

int TCPSocket::getPort() const
{
	return (m_port);
}

string TCPSocket::getIP() const
{
	return (m_ip);
}

int TCPSocket::getFD() const
{
	return (m_sfd);
}

bool TCPSocket::isValid()
{
	if (fcntl(m_sfd, F_GETFL) == -1) return (false);
	else
		return (true);
}

TCPSocket::TCPSocket(int sfd, struct sockaddr_in* address)
	: m_sfd(sfd)
{
	char ip[17];
	inet_ntop(AF_INET, (struct in_addr*) &(address->sin_addr.s_addr), ip,
		sizeof(ip) - 1);
	m_ip = ip;
	m_port = ntohs(address->sin_port);
}

