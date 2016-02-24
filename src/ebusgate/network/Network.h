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

#ifndef NETWORK_NETWORK_H
#define NETWORK_NETWORK_H

#include "Connection.h"
#include "TCPServer.h"

#include <string>
#include <thread>
#include <list>

using std::list;
using std::thread;

class Network
{

public:
	Network(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue);

	~Network();

	void start();
	void stop();

private:
	Network(const Network&);
	Network& operator=(const Network&);

	thread m_thread;

	list<Connection*> m_connections;

	NQueue<NetMessage*>* m_netMsgQueue;

	TCPServer* m_tcpServer;

	PipeNotify m_notify;

	bool m_listening;

	void run();

	void cleanConnections();

};

#endif // NETWORK_NETWORK_H

