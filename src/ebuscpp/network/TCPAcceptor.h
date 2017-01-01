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

#ifndef NETWORK_TCPACCEPTOR_H
#define NETWORK_TCPACCEPTOR_H

#include "TCPConnection.h"
#include "Server.h"

#include <string>
#include <thread>
#include <list>

using libutils::NQueue;
using std::list;
using std::thread;

class TCPAcceptor
{

public:
	TCPAcceptor(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue);
	~TCPAcceptor();

	void start();
	void stop();

private:
	TCPAcceptor(const TCPAcceptor&);
	TCPAcceptor& operator=(const TCPAcceptor&);

	thread m_thread;

	list<TCPConnection*> m_connections;

	NQueue<NetMessage*>* m_netMsgQueue;

	Server* m_tcpServer = nullptr;

	PipeNotify m_notify;

	bool m_running;

	void run();

	void cleanConnections();

};

#endif // NETWORK_TCPACCEPTOR_H

