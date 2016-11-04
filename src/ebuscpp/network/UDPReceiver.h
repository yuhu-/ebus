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

#ifndef NETWORK_UDPRECEIVER_H
#define NETWORK_UDPRECEIVER_H

#include "PipeNotify.h"
#include "NQueue.h"
#include "Server.h"

#include <thread>
#include "../../ebuscpp/network/NetMessage.h"

using std::thread;

class UDPReceiver
{

public:
	UDPReceiver(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue);
	~UDPReceiver();

	void start();
	void stop();

private:
	UDPReceiver(const UDPReceiver&);
	UDPReceiver& operator=(const UDPReceiver&);

	thread m_thread;

	NQueue<NetMessage*>* m_netMsgQueue;

	Server* m_udpServer = nullptr;

	Socket* m_socket = nullptr;

	PipeNotify m_notify;

	bool m_running;

	int m_ids = 0;

	void run();

};

#endif // NETWORK_UDPRECEIVER_H

