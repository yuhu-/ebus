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

#include "EbusProcess.h"

#include "Common.h"

#include <iomanip>

using libebus::slaveAddress;
using std::ostringstream;
using std::endl;

EbusProcess::EbusProcess(const unsigned char address)
	: Notify(), m_address(address), m_slaveAddress(slaveAddress(address))
{
}

EbusProcess::~EbusProcess()
{
	while (m_ebusMsgQueue.size() > 0)
		delete m_ebusMsgQueue.dequeue();
}

void EbusProcess::start()
{
	m_thread = thread(&EbusProcess::run, this);
}

void EbusProcess::stop()
{
	if (m_thread.joinable())
	{
		m_running = false;
		notify();
		m_thread.join();
	}
}

void EbusProcess::enqueueMessage(EbusMessage* message)
{
	IEbusProcess::enqueueMessage(message);
}

void EbusProcess::createMessage(EbusSequence& eSeq)
{
	if (eSeq.getMasterState() == EBUS_OK) m_ebusMsgQueue.enqueue(new EbusMessage(eSeq));
}

EbusMessage* EbusProcess::processMessage()
{
	EbusMessage* ebusMessage = m_ebusMsgQueue.dequeue();
	enqueueMessage(ebusMessage);
	ebusMessage->waitNotify();
	return (ebusMessage);
}

size_t EbusProcess::pendingMessages()
{
	return (m_ebusMsgQueue.size());
}
