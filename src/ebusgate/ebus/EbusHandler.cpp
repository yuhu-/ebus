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

using std::ios;

extern Logger& L;

EbusHandler::EbusHandler(const unsigned char address, const string device,
	const bool noDeviceCheck, const long reopenTime,
	const long arbitrationTime, const long receiveTimeout,
	const int lockCounter, const int lockRetries, const bool active,
	const bool store, const bool logRaw, const bool dumpRaw,
	const char* dumpRawFile, const long dumpRawFileMaxSize)
	: m_address(address), m_reopenTime(reopenTime), m_arbitrationTime(
		arbitrationTime), m_receiveTimeout(receiveTimeout), m_lockCounter(
		lockCounter), m_lockRetries(lockRetries), m_active(active), m_store(
		store), m_lastResult(DEV_OK), m_logRaw(logRaw), m_dumpRawFile(
		dumpRawFile), m_dumpRawFileMaxSize(dumpRawFileMaxSize)
{
	m_device = new EbusDevice(device, noDeviceCheck);
	changeState(Connect::getInstance());

	setDumpRaw(dumpRaw);

	m_ebusDataStore = new EbusDataStore();
}

EbusHandler::~EbusHandler()
{
	if (m_device != nullptr)
	{
		delete m_device;
		m_device = nullptr;
	}

	m_dumpRawStream.close();

	delete m_ebusDataStore;
}

void EbusHandler::start()
{
	m_thread = thread(&EbusHandler::run, this);
}

void EbusHandler::stop()
{
	m_forceState = Idle::getInstance();
	notify();
	m_running = false;
	m_thread.join();
}

void EbusHandler::open()
{
	m_forceState = Connect::getInstance();
	notify();
}

void EbusHandler::close()
{
	m_forceState = Idle::getInstance();
}

bool EbusHandler::getLogRaw()
{
	return (m_logRaw);
}

void EbusHandler::setLogRaw(const bool logRaw)
{
	m_logRaw = logRaw;
}

bool EbusHandler::getDumpRaw() const
{
	return (m_dumpRaw);
}

void EbusHandler::setDumpRaw(const bool dumpRaw)
{
	if (dumpRaw == m_dumpRaw) return;

	m_dumpRaw = dumpRaw;

	if (dumpRaw == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpRawFile.c_str(),
			ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

void EbusHandler::setDumpRawFile(const string& dumpFile)
{
	if (dumpFile == m_dumpRawFile) return;

	m_dumpRawStream.close();
	m_dumpRawFile = dumpFile;

	if (m_dumpRaw == true)
	{
		m_dumpRawStream.open(m_dumpRawFile.c_str(),
			ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

void EbusHandler::setDumpRawMaxSize(const long maxSize)
{
	m_dumpRawFileMaxSize = maxSize;
}

void EbusHandler::addMessage(EbusMessage* message)
{
	m_ebusMsgQueue.enqueue(message);
}

const string EbusHandler::grabMessage(const string& str) const
{
	return (m_ebusDataStore->read(str));
}

void EbusHandler::run()
{
	while (m_running == true)
	{
		m_lastResult = m_state->run(this);
		if (m_lastResult != DEV_OK) changeState(OnError::getInstance());

		if (m_forceState != nullptr)
		{
			changeState(m_forceState);
			m_forceState = nullptr;
		}
	}
}

void EbusHandler::changeState(State* state)
{
	if (m_state != state)
	{
		m_state = state;
		L.log(trace, "State: %s", m_state->toString());
	}
}

