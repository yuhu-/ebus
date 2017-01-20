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

#include "Proxy.h"
#include "Logger.h"

#include <unistd.h>

Proxy::Proxy(const unsigned char address)
	: EbusProcess(address)
{
	m_forward = new Forward();
	m_forward->start();
}

Proxy::~Proxy()
{
	if (m_forward != nullptr)
	{
		m_forward->stop();
		delete m_forward;
		m_forward = nullptr;
	}
}

void Proxy::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	if (remove == true)
		m_forward->remove(ip, port, filter, result);
	else
		m_forward->append(ip, port, filter, result);
}

void Proxy::run()
{
	LIBLOGGER_INFO("Proxy started");

	while (m_running == true)
	{
		//waitNotify();

		sleep(1);
		LIBLOGGER_INFO("notified");
		if (pendingMessages() != 0)
		{
			EbusMessage* ebusMessage = processMessage();
			LIBLOGGER_INFO(ebusMessage->getResult());
			delete ebusMessage;
		}
	}

	LIBLOGGER_INFO("Proxy stopped");
}

Action Proxy::getEvaluatedAction(EbusSequence& eSeq)
{
	LIBLOGGER_INFO("search %s", eSeq.toStringLog().c_str());

	if (eSeq.getMaster().contains("0700") == true)
	{
		return (Action::ignore);
	}

	if (eSeq.getMaster().contains("0704") == true)
	{
		eSeq.createSlave("0a7a50524f585901010101");
		return (Action::response);
	}

	if (eSeq.getMaster().contains("07fe") == true)
	{
		eSeq.clear();
		eSeq.createMaster(m_address, BROADCAST, "07ff00");
		createMessage(eSeq);
		return (Action::ignore);
	}

	if (eSeq.getMaster().contains("b505") == true)
	{
		return (Action::ignore);
	}

	if (eSeq.getMaster().contains("b516") == true)
	{
		return (Action::ignore);
	}

	return (Action::undefined);
}

void Proxy::evalActiveMessage(EbusSequence& eSeq)
{
	if (m_forward->isActive())
	{
		LIBLOGGER_INFO("forward %s", eSeq.toStringLog().c_str());
		m_forward->enqueue(eSeq);
	}
}

void Proxy::evalPassiveMessage(EbusSequence& eSeq)
{
	if (m_forward->isActive())
	{
		LIBLOGGER_INFO("forward %s", eSeq.toStringLog().c_str());
		m_forward->enqueue(eSeq);
	}
}
