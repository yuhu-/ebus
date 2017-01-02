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

#include "TCPAcceptor.h"
#include "Logger.h"

#include <cstring>
#include <iostream>

#include <poll.h>

TCPAcceptor::TCPAcceptor(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue)
	: m_netMsgQueue(netMsgQueue), m_running(false)
{
	if (local == true)
		m_tcpServer = new Server("127.0.0.1", port);
	else
		m_tcpServer = new Server("0.0.0.0", port);

	if (m_tcpServer != nullptr && m_tcpServer->start() == 0) m_running = true;

}

TCPAcceptor::~TCPAcceptor()
{
	while (m_connections.size() > 0)
	{
		TCPConnection* connection = m_connections.back();
		m_connections.pop_back();
		connection->stop();
		delete connection;
	}

	delete m_tcpServer;
	m_tcpServer = nullptr;
}

void TCPAcceptor::start()
{
	m_thread = thread(&TCPAcceptor::run, this);
}

void TCPAcceptor::stop()
{
	if (m_thread.joinable())
	{
		m_notify.notify();
		m_thread.join();
	}
}

void TCPAcceptor::run()
{
	if (m_running == false) return;

	LIBLOGGER_INFO("started listening on %s", m_tcpServer->toString().c_str());

	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 1;
	tdiff.tv_nsec = 0;

	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(struct pollfd) * nfds);

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	fds[1].fd = m_tcpServer->getFD();
	fds[1].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	while (true)
	{
		// wait for new fd event
		int ret = ppoll(fds, nfds, &tdiff, nullptr);

		if (ret == 0)
		{
			cleanConnections();
			continue;
		}

		// new data from notify
		if ((fds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLRDHUP))
			|| (fds[1].revents & (POLLERR | POLLHUP | POLLRDHUP))) break;

		// new data from socket
		if (fds[1].revents & POLLIN)
		{
			Socket* socket = m_tcpServer->newSocket();
			if (socket == nullptr) continue;

			TCPConnection* connection = new TCPConnection(socket, m_netMsgQueue);
			if (connection == nullptr) continue;

			connection->start();
			m_connections.push_back(connection);
		}

	}

	LIBLOGGER_INFO("stopped listening");
}

void TCPAcceptor::cleanConnections()
{
	for (auto c_it = m_connections.begin(); c_it != m_connections.end(); c_it++)
	{
		if ((*c_it)->isClosed() == true)
		{
			TCPConnection* connection = *c_it;
			c_it = m_connections.erase(c_it);
			connection->stop();
			delete connection;
			LIBLOGGER_TRACE("dead connection removed - %d", m_connections.size());
		}
	}
}

