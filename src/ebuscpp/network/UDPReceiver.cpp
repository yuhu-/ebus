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

#include "UDPReceiver.h"
#include "Logger.h"

#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <poll.h>

UDPReceiver::UDPReceiver(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue)
	: m_netMsgQueue(netMsgQueue), m_running(false)
{
	if (local == true)
		m_udpServer = new Server("127.0.0.1", port, true);
	else
		m_udpServer = new Server("0.0.0.0", port, true);

	if (m_udpServer != nullptr && m_udpServer->start() == 0)
	{
		m_socket = m_udpServer->newSocket();
		if (m_socket != nullptr) m_running = true;
	}

}

UDPReceiver::~UDPReceiver()
{
	delete m_udpServer;
	m_udpServer = nullptr;
}

void UDPReceiver::start()
{
	m_thread = thread(&UDPReceiver::run, this);
}

void UDPReceiver::stop()
{
	if (m_thread.joinable())
	{
		m_notify.notify();
		m_thread.join();
	}
}

void UDPReceiver::run()
{
	LIBLOGGER_INFO("started UDP listening on %s", m_udpServer->toString().c_str());

	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 2;
	tdiff.tv_nsec = 0;

	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(struct pollfd) * nfds);

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	fds[1].fd = m_udpServer->getFD();
	fds[1].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	while (true)
	{
		// wait for new fd event
		int ret = ppoll(fds, nfds, &tdiff, nullptr);

		bool newData = false;
		if (ret != 0)
		{
			// new data from notify
			if (ret < 0 || (fds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLRDHUP))
				|| (fds[1].revents & (POLLERR | POLLHUP | POLLRDHUP))) break;

			// new data from socket
			newData = fds[1].revents & POLLIN;
		}

		if (newData == true)
		{
			m_ids++;

			struct sockaddr_in sock;
			socklen_t socklen = sizeof(struct sockaddr_in);
			char data[1024];
			ssize_t datalen;

			memset(data, '\0', sizeof(data));

			if (m_socket->isValid() == true)
				datalen = m_socket->recv(data, sizeof(data) - 1, &sock, &socklen);
			else
				break;

			char ip[17];
			inet_ntop(AF_INET, (struct in_addr*) &(sock.sin_addr.s_addr), ip, sizeof(ip) - 1);
			long port = ntohs(sock.sin_port);

			LIBLOGGER_INFO("[%05d] %s UDP opened", m_ids, ip);

			// removed closed socket
			if (datalen <= 0 || strncasecmp(data, "QUIT", 4) == 0)
			{
				LIBLOGGER_INFO("[%05d] UDP connection closed", m_ids);
				continue;
			}

			// decode client data
			data[datalen] = '\0';
			NetMessage message(data, ip, port);
			m_netMsgQueue->enqueue(&message);

			// wait for result
			message.waitNotify();
			string result = message.getResult();

			if (m_socket->isValid() == false) break;

			m_socket->send(result.c_str(), result.size(), (const struct sockaddr_in*) &sock, socklen);

			LIBLOGGER_INFO("[%05d] UDP connection closed", m_ids);
		}

	}

	if (m_socket != nullptr)
	{
		delete m_socket;
		m_socket = nullptr;
	}

	LIBLOGGER_INFO("stopped UDP listening");
}

