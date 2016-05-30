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

#include "ProcessHandler.h"

#include "Logger.h"

#include <iomanip>

using std::ostringstream;
using std::endl;

ProcessHandler::ProcessHandler(const unsigned char address)
	: Notify(), m_address(address)
{
	m_forwardHandler = new ForwardHandler();
	m_forwardHandler->start();
}

ProcessHandler::~ProcessHandler()
{
	if (m_forwardHandler != nullptr)
	{
		m_forwardHandler->stop();
		delete m_forwardHandler;
		m_forwardHandler = nullptr;
	}
}

void ProcessHandler::start()
{
	m_thread = thread(&ProcessHandler::run, this);
}

void ProcessHandler::stop()
{
	if (m_thread.joinable())
	{
		m_running = false;
		notify();
		m_thread.join();
	}
}

ProcessType ProcessHandler::active(EbusSequence& eSeq)
{
	Logger logger = Logger("ProcessHandler::active");
	logger.info("search %s", eSeq.toStringLog().c_str());

	if (eSeq.getMaster().contains("0700") == true)
	{
		return (pt_ignore);
	}

	if (eSeq.getMaster().contains("0704") == true)
	{
		eSeq.createSlave("0a7a454741544501010101");
		return (pt_response);
	}

	if (eSeq.getMaster().contains("07fe") == true)
	{
		eSeq.clear();
		eSeq.createMaster(m_address, BROADCAST, "07ff00");
		return (pt_send);
	}

	if (eSeq.getMaster().contains("b505") == true)
	{
		return (pt_ignore);
	}

	if (eSeq.getMaster().contains("b516") == true)
	{
		return (pt_ignore);
	}

	return (pt_undefined);
}

void ProcessHandler::passive(EbusSequence& eSeq)
{
	Logger logger = Logger("ProcessHandler::passive");
	logger.info("forward %s", eSeq.toStringLog().c_str());

	m_forwardHandler->enqueue(eSeq);

	// TODO handle passive message

}

void ProcessHandler::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	if (remove == true)
		m_forwardHandler->remove(ip, port, filter, result);
	else
		m_forwardHandler->append(ip, port, filter, result);
}

void ProcessHandler::run()
{
	Logger logger = Logger("ProcessHandler::run");
	logger.info("started");

	while (m_running == true)
	{
		waitNotify();
		// TODO implement business logic
	}

	logger.info("stopped");
}

