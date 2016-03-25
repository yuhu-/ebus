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

#include "Socket.h"

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

Socket::~Socket()
{
	close(m_sfd);
}

ssize_t Socket::send(const char* buffer, const size_t len, const struct sockaddr_in* address, const socklen_t addrlen)
{
	return (sendto(m_sfd, buffer, len, MSG_NOSIGNAL, (const struct sockaddr*) address, addrlen));
}
ssize_t Socket::recv(char* buffer, const size_t len, struct sockaddr_in* address, socklen_t* addrlen)
{
	return (recvfrom(m_sfd, buffer, len, 0, (struct sockaddr*) address, addrlen));
}

string Socket::getIP() const
{
	return (m_ip);
}

long Socket::getPort() const
{
	return (m_port);
}

int Socket::getFD() const
{
	return (m_sfd);
}

bool Socket::isValid()
{
	if (fcntl(m_sfd, F_GETFL) == -1)
		return (false);
	else
		return (true);
}

Socket::Socket(const int sfd, const struct sockaddr_in* address)
	: m_sfd(sfd)
{
	char ip[INET_ADDRSTRLEN];
	m_ip = inet_ntop(AF_INET, (const struct in_addr*) &(address->sin_addr.s_addr), ip, INET_ADDRSTRLEN);
	m_port = ntohs(address->sin_port);
}

