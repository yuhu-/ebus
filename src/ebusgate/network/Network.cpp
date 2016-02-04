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

#include "Network.h"
#include "Logger.h"

#include <cstring>
#include <iostream>

#include <poll.h>

extern Logger& L;

Network::Network(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue)
	: m_netMsgQueue(netMsgQueue), m_listening(false)
{
	if (local == true)
	{
		m_tcpServer = new TCPServer(port, "127.0.0.1");
	}
	else
	{
		m_tcpServer = new TCPServer(port, "0.0.0.0");
	}

	if (m_tcpServer != nullptr && m_tcpServer->start() == 0) m_listening = true;

}

Network::~Network()
{
	while (m_connections.size() > 0)
	{
		Connection* connection = m_connections.back();
		m_connections.pop_back();
		connection->stop();
		delete connection;
	}

	delete m_tcpServer;
	m_tcpServer = nullptr;
}

void Network::start()
{
	m_thread = thread(&Network::run, this);
}

void Network::stop()
{
	if (m_thread.joinable())
	{
		m_notify.notify();
		m_thread.join();
	}
}

void Network::run()
{
	if (m_listening == false) return;

	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 1;
	tdiff.tv_nsec = 0;

	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(struct pollfd) * nfds);

	fds[0].fd = m_notify.notifyFD();
	fds[0].events = POLLIN;

	fds[1].fd = m_tcpServer->getFD();
	fds[1].events = POLLIN;

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
		if (fds[0].revents & POLLIN) break;

		// new data from socket
		if (fds[1].revents & POLLIN)
		{

			TCPSocket* socket = m_tcpServer->newSocket();
			if (socket == nullptr) continue;

			Connection* connection = new Connection(socket, m_netMsgQueue);
			if (connection == nullptr) continue;

			L.log(info, "[%05d] %s opened", connection->getID(), socket->getIP().c_str());

			connection->start();
			m_connections.push_back(connection);
		}

	}

}

void Network::cleanConnections()
{
	for (auto c_it = m_connections.begin(); c_it != m_connections.end(); c_it++)
	{
		if ((*c_it)->isClosed() == true)
		{
			Connection* connection = *c_it;
			c_it = m_connections.erase(c_it);
			connection->stop();
			delete connection;
			L.log(trace, "dead connection removed - %d", m_connections.size());
		}
	}
}

