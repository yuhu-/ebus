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

#include "Connection.h"
#include "Logger.h"

#include <cstring>
#include <iostream>

#include <poll.h>

extern Logger& L;

int Connection::m_ids = 0;

Connection::Connection(TCPSocket* socket, NQueue<NetMessage*>* netMessages)
	: m_socket(socket), m_netMessages(netMessages)
{
	m_id = ++m_ids;
}

void Connection::start()
{
	m_thread = thread(&Connection::run, this);
}

void Connection::stop()
{
	if (m_thread.joinable())
	{
		m_notify.notify();
		m_thread.join();
	}
}

bool Connection::isClosed() const
{
	return (m_closed);
}

int Connection::getID() const
{
	return (m_id);
}

void Connection::run()
{
	struct timespec tdiff;

	// set timeout
	tdiff.tv_sec = 2;
	tdiff.tv_nsec = 0;
	int notifyFD = m_notify.notifyFD();
	int sockFD = m_socket->getFD();

	int nfds = 2;
	struct pollfd fds[nfds];

	memset(fds, 0, sizeof(struct pollfd) * nfds);

	fds[0].fd = notifyFD;
	fds[0].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	fds[1].fd = sockFD;
	fds[1].events = POLLIN | POLLERR | POLLHUP | POLLRDHUP;

	while (m_closed == false)
	{

		// wait for new fd event
		int ret = ppoll(fds, nfds, &tdiff, NULL);

		bool newData = false;
		if (ret != 0)
		{

			// new data from notify
			if (ret < 0
				|| (fds[0].revents
					& (POLLIN | POLLERR | POLLHUP
						| POLLRDHUP))
				|| (fds[1].revents & (POLLERR | POLLHUP)))
				break;

			// new data from socket
			newData = fds[1].revents & POLLIN;
			m_closed = fds[1].revents & POLLRDHUP;

		}

		if (newData == true || m_listening == true)
		{
			char data[256];
			ssize_t datalen = 0;

			if (newData == true)
			{
				if (m_socket->isValid() == true)
				{
					datalen = m_socket->recv(data,
						sizeof(data) - 1);
				}

				else
				{
					break;
				}

				// removed closed socket
				if (datalen <= 0
					|| strncasecmp(data, "QUIT", 4) == 0)
					break;

			}

			// decode client data
			data[datalen] = '\0';
			NetMessage message(data);
			m_netMessages->enqueue(&message);

			// wait for result
			message.waitNotify();
			string result = message.getResult();

			if (m_socket->isValid() == false) break;

			m_socket->send(result.c_str(), result.size());
		}

	}

	if (m_socket != nullptr)
	{
		delete m_socket;
		m_socket = nullptr;
	}

	m_closed = true;
	L.log(info, "[%05d] connection closed", getID());
}

