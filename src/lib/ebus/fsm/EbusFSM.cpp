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

#include "EbusFSM.h"
#include "Connect.h"
#include "Idle.h"
#include "OnError.h"
#include "State.h"
#include "Color.h"

#include <sstream>
#include <algorithm>

using namespace libebus;
using namespace libutils;

using std::ios;
using std::pair;
using std::ostringstream;
using std::copy_n;
using std::back_inserter;

EbusFSM::EbusFSM(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
	const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
	const bool dump, const string dumpFile, const long dumpFileMaxSize, IProcess* process, ILogger* logger)
	: Notify(), m_address(address), m_reopenTime(reopenTime), m_arbitrationTime(arbitrationTime), m_receiveTimeout(
		receiveTimeout), m_lockCounter(lockCounter), m_lockRetries(lockRetries), m_lastResult(
	DEV_OK), m_dumpFile(dumpFile), m_dumpFileMaxSize(dumpFileMaxSize), m_process(process), m_logger(logger)
{
	m_ebusDevice = new EbusDevice(device, noDeviceCheck);
	changeState(Connect::getConnect());

	setDump(dump);
}

EbusFSM::~EbusFSM()
{
	if (m_ebusDevice != nullptr)
	{
		delete m_ebusDevice;
		m_ebusDevice = nullptr;
	}

	m_dumpRawStream.close();
}

void EbusFSM::start()
{
	m_thread = thread(&EbusFSM::run, this);
}

void EbusFSM::stop()
{
	m_forceState = Idle::getIdle();
	notify();
	m_running = false;
	m_thread.join();
}

void EbusFSM::open()
{
	m_forceState = Connect::getConnect();
	notify();
}

void EbusFSM::close()
{
	m_forceState = Idle::getIdle();
}

bool EbusFSM::getDump() const
{
	return (m_dump);
}

void EbusFSM::setDump(bool dump)
{
	if (dump == m_dump) return;

	m_dump = dump;

	if (dump == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpFile.c_str(), ios::binary | ios::app);
		m_dumpFileSize = 0;
	}
}

void EbusFSM::enqueue(EbusMessage* message)
{
	m_ebusMsgQueue.enqueue(message);
}

void EbusFSM::run()
{
	m_logger->info("FSM started");

	while (m_running == true)
	{
		m_lastResult = m_state->run(this);
		if (m_lastResult != DEV_OK) changeState(OnError::getOnError());

		if (m_forceState != nullptr)
		{
			changeState(m_forceState);
			m_forceState = nullptr;
		}
	}

	m_logger->info("FSM stopped");
}

void EbusFSM::changeState(State* state)
{
	if (m_state != state)
	{
		m_state = state;
		ostringstream ostr;
		ostr << color::cyan << m_state->toString() << color::reset;
		m_logger->debug(ostr.str());
	}
}

