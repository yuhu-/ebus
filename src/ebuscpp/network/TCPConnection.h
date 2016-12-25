/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#ifndef NETWORK_TCPCONNECTION_H
#define NETWORK_TCPCONNECTION_H

#include "NetMessage.h"
#include "PipeNotify.h"
#include "NQueue.h"
#include "Socket.h"

#include <thread>

using libutils::NQueue;
using libutils::PipeNotify;
using std::thread;

class TCPConnection
{

public:
	TCPConnection(Socket* socket, NQueue<NetMessage*>* netMsgQueue);

	void start();
	void stop();

	bool isClosed() const;

private:
	thread m_thread;

	bool m_closed = false;

	Socket* m_socket = nullptr;

	NQueue<NetMessage*>* m_netMsgQueue;

	PipeNotify m_notify;

	int m_id;

	static int m_ids;

	void run();

};

#endif // NETWORK_TCPCONNECTION_H

