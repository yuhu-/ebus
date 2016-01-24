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

#ifndef NETWORK_CONNECTION_H
#define NETWORK_CONNECTION_H

#include "NetMessage.h"
#include "TCPSocket.h"
#include "PipeNotify.h"
#include "NQueue.h"

#include <thread>

using std::thread;

class Connection
{

public:
	Connection(TCPSocket* socket, NQueue<NetMessage*>* netMsgQueue);

	void start();
	void stop();

	bool isClosed() const;

	int getID() const;

private:
	thread m_thread;

	bool m_closed = false;

	TCPSocket* m_socket;

	NQueue<NetMessage*>* m_netMsgQueue;

	PipeNotify m_notify;

	int m_id;

	static int m_ids;

	bool m_listening = false;

	void run();

};

#endif // NETWORK_CONNECTION_H

