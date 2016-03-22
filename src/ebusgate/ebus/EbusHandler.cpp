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

#include "EbusHandler.h"
#include "State.h"
#include "Idle.h"
#include "Connect.h"
#include "OnError.h"
#include "Logger.h"

#include <sstream>
#include <algorithm>

using std::ios;
using std::pair;
using std::ostringstream;
using std::copy_n;
using std::back_inserter;

EbusHandler::EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck,
	const long reopenTime, const long arbitrationTime, const long receiveTimeout, const int lockCounter,
	const int lockRetries, const bool active, const bool dumpRaw, const string dumpRawFile,
	const long dumpRawFileMaxSize, const bool logRaw)
	: m_address(address), m_reopenTime(reopenTime), m_arbitrationTime(arbitrationTime), m_receiveTimeout(
		receiveTimeout), m_lockCounter(lockCounter), m_lockRetries(lockRetries), m_active(active), m_lastResult(
	DEV_OK), m_dumpRawFile(dumpRawFile), m_dumpRawFileMaxSize(dumpRawFileMaxSize), m_logRaw(logRaw)
{
	m_dataHandler = new DataHandler();
	m_dataHandler->start();

	m_device = new EbusDevice(device, noDeviceCheck);
	changeState(Connect::getConnect());

	setDumpRaw(dumpRaw);

	if (m_active == true)
	{
		EbusSequence eSeq;
		eSeq.createMaster(m_address, BROADCAST, "07040a7a454741544501010101");

		if (eSeq.getMasterState() == EBUS_OK) enqueue(new EbusMessage(eSeq));
	}
}

EbusHandler::~EbusHandler()
{
	if (m_device != nullptr)
	{
		delete m_device;
		m_device = nullptr;
	}

	m_dumpRawStream.close();

	if (m_dataHandler != nullptr)
	{
		m_dataHandler->stop();
		delete m_dataHandler;
		m_dataHandler = nullptr;
	}
}

void EbusHandler::start()
{
	m_thread = thread(&EbusHandler::run, this);
}

void EbusHandler::stop()
{
	m_forceState = Idle::getIdle();
	notify();
	m_running = false;
	m_thread.join();
}

void EbusHandler::open()
{
	m_forceState = Connect::getConnect();
	notify();
}

void EbusHandler::close()
{
	m_forceState = Idle::getIdle();
}

bool EbusHandler::getActive()
{
	return (m_active);
}

void EbusHandler::setActive(bool active)
{
	m_active = active;
}

bool EbusHandler::getDumpRaw() const
{
	return (m_dumpRaw);
}

void EbusHandler::setDumpRaw(bool dumpRaw)
{
	if (dumpRaw == m_dumpRaw) return;

	m_dumpRaw = dumpRaw;

	if (dumpRaw == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpRawFile.c_str(), ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

bool EbusHandler::getLogRaw()
{
	return (m_logRaw);
}

void EbusHandler::setLogRaw(bool logRaw)
{
	m_logRaw = logRaw;
}

void EbusHandler::enqueue(EbusMessage* message)
{
	m_ebusMsgQueue.enqueue(message);
}

bool EbusHandler::subscribe(const string& ip, long port, const string& filter, ostringstream& result)
{
	return (m_dataHandler->subscribe(ip, port, filter, result));
}

bool EbusHandler::unsubscribe(const string& ip, long port, const string& filter, ostringstream& result)
{
	return (m_dataHandler->unsubscribe(ip, port, filter, result));
}

void EbusHandler::run()
{
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
}

void EbusHandler::changeState(State* state)
{
	Logger logger = Logger("EbusHandler::changeState");

	if (m_state != state)
	{
		m_state = state;
		logger.trace("%s", m_state->toString().c_str());
	}
}


