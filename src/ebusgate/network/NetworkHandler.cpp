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

#include "NetworkHandler.h"

NetworkHandler::NetworkHandler(const bool local, const int port, NQueue<NetMessage*>* netMsgQueue)
{
	m_tcpAcceptor = new TCPAcceptor(local, port, netMsgQueue);
	m_tcpAcceptor->start();

	m_udpReceiver = new UDPReceiver(local, port, netMsgQueue);
	m_udpReceiver->start();
}

NetworkHandler::~NetworkHandler()
{
	if (m_udpReceiver != nullptr)
	{
		m_udpReceiver->stop();
		delete m_udpReceiver;
		m_udpReceiver = nullptr;
	}

	if (m_tcpAcceptor != nullptr)
	{
		m_tcpAcceptor->stop();
		delete m_tcpAcceptor;
		m_tcpAcceptor = nullptr;
	}
}



