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
#include "EbusCommon.h"

#include <iomanip>

using libebus::slaveAddress;
using std::ostringstream;
using std::endl;

libebus::EbusProcess::EbusProcess(const unsigned char address)
	: Notify(), m_address(address), m_slaveAddress(slaveAddress(address))
{
}

libebus::EbusProcess::~EbusProcess()
{
	while (queuedMessages() > 0)
		delete dequeueMessage();
}

void libebus::EbusProcess::start()
{
	m_thread = thread(&EbusProcess::run, this);
}

void libebus::EbusProcess::stop()
{
	if (m_thread.joinable())
	{
		m_running = false;
		notify();
		m_thread.join();
	}
}

const string libebus::EbusProcess::sendMessage(const string& message)
{
	ostringstream result;
	EbusSequence eSeq;
	eSeq.createMaster(m_address, message);

	if (eSeq.getMasterState() == EBUS_OK)
	{
		EbusMessage* ebusMessage = new EbusMessage(eSeq);
		enqueueMessage(ebusMessage);
		ebusMessage->waitNotify();
		result << ebusMessage->getResult();
		delete ebusMessage;
	}
	else
	{
		result << eSeq.toStringMaster();
	}

	return (result.str());
}

void libebus::EbusProcess::createMessage(EbusSequence& eSeq)
{
	if (eSeq.getMasterState() == EBUS_OK) enqueueMessage(new EbusMessage(eSeq));
}

libebus::EbusMessage* libebus::EbusProcess::processMessage()
{
	EbusMessage* ebusMessage = dequeueMessage();
	enqueueMessage(ebusMessage);
	ebusMessage->waitNotify();
	return (ebusMessage);
}
